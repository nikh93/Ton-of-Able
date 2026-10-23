#include "RigChain.h"

#include <optional>

#include <algorithm>

#include "presets/PresetManager.h"

using Graph = juce::AudioProcessorGraph;
using IO = Graph::AudioGraphIOProcessor;

RigChain::RigChain()
{
    graph.setPlayConfigDetails(2, 2, 48000.0, 512);
    audioIn = graph.addNode(std::make_unique<IO>(IO::audioInputNode), std::nullopt, Graph::UpdateKind::none)->nodeID;
    audioOut = graph.addNode(std::make_unique<IO>(IO::audioOutputNode), std::nullopt, Graph::UpdateKind::none)->nodeID;
    midiIn = graph.addNode(std::make_unique<IO>(IO::midiInputNode), std::nullopt, Graph::UpdateKind::none)->nodeID;
    graphMidi.ensureSize(8192);
    setLayout(chain::Layout::standard());
}

RigChain::~RigChain()
{
    graph.releaseResources();
    graph.clear();
}

chainnodes::Builtin::Process RigChain::builtinProcess(const juce::String& kind)
{
    const auto onBuffer = [](auto& processor)
    {
        return [&processor](float* mono, int count)
        {
            juce::AudioBuffer<float> view(&mono, 1, count);
            processor.process(view);
        };
    };
    if (kind == "pitch") return [this](float* mono, int count) { pitchShifter.process(mono, count); };
    if (kind == "compressor") return onBuffer(compressor);
    if (kind == "wah") return onBuffer(wahPedal);
    if (kind == "drive") return onBuffer(driveProcessor);
    if (kind == "pedal") return [this](float* mono, int count) { namPedalProcessor.process(mono, count); };
    if (kind == "amp")
        return [this](float* mono, int count)
        {
            // The NAM amp is bypassed until the loader thread has supplied a prepared model.
            auto* dry = namDryBuffer.getWritePointer(0);
            juce::FloatVectorOperations::copy(dry, mono, count);
            for (int sample = 0; sample < count; ++sample)
                mono[sample] *= juce::Decibels::decibelsToGain(namInputTrim.next());
            namProcessor.process(mono, count);
            for (int sample = 0; sample < count; ++sample)
                mono[sample] = dry[sample] + namBypassRamp.nextWetMix() * (mono[sample] - dry[sample]);
        };
    if (kind == "ir") return onBuffer(irProcessor);
    if (kind == "ampEq") return onBuffer(eqProcessor);
    if (kind == "geq") return onBuffer(graphicEq);
    if (kind == "autoWah") return onBuffer(autoWahProcessor);
    if (kind == "chorus") return onBuffer(chorusProcessor);
    if (kind == "delay") return onBuffer(delayProcessor);
    if (kind == "reverb") return onBuffer(reverbProcessor);
    jassertfalse;
    return [](float*, int) {};
}

namespace
{
juce::String pluginKey(const juce::ValueTree& state)
{
    return state.getProperty("descriptionXml").toString() + "|" + state.getProperty("stateBase64").toString();
}
} // namespace

void RigChain::setLayout(const chain::Layout& layout)
{
    errors.clear();
    // Blocks that are gone (or changed their kind) leave the graph.
    for (auto it = blocks.begin(); it != blocks.end();)
    {
        const auto* node = layout.find(it->first);
        if (node == nullptr || node->kind != it->second.kind)
        {
            graph.removeNode(it->second.node, Graph::UpdateKind::none);
            it = blocks.erase(it);
        }
        else
            ++it;
    }
    const auto block = maximumBlock > 0 ? maximumBlock : 512;
    for (const auto* list : { &layout.serial, &layout.side })
        for (const auto& node : *list)
        {
            auto existing = blocks.find(node.id);
            if (existing == blocks.end())
            {
                std::unique_ptr<juce::AudioProcessor> processor;
                if (node.isBuiltin())
                    processor = std::make_unique<chainnodes::Builtin>(builtinProcess(node.kind));
                else if (node.kind == "vst3")
                {
                    auto plugin = std::make_unique<chainnodes::Plugin>();
                    plugin->slot.prepare(preparedRate, block); // a slot loads only once prepared
                    processor = std::move(plugin);
                }
                else
                    processor = std::make_unique<chainnodes::Transpose>();
                const auto added = graph.addNode(std::move(processor), std::nullopt, Graph::UpdateKind::none);
                if (added == nullptr)
                    continue;
                existing = blocks.emplace(node.id, Block { added->nodeID, node.kind, {} }).first;
            }
            auto* processor = graph.getNodeForId(existing->second.node)->getProcessor();
            if (auto* plugin = dynamic_cast<chainnodes::Plugin*>(processor))
            {
                if (const auto key = pluginKey(node.plugin); key != existing->second.loadedState)
                {
                    existing->second.loadedState = key; // also after a failure: no reload loop
                    auto state = node.plugin.createCopy();
                    state.setProperty("bypassed", node.bypassed, nullptr);
                    if (state.isValid())
                        if (const auto result = plugin->slot.restoreState(state); result.failed())
                            errors.add(node.name + ": " + result.getErrorMessage());
                }
                plugin->slot.setBypassed(node.bypassed);
            }
            else if (auto* transpose = dynamic_cast<chainnodes::Transpose*>(processor))
            {
                transpose->semitones.store(node.semitones);
                transpose->bypassed.store(node.bypassed);
            }
        }

    // Rewire: the serial path, then the extra links (each through its own level).
    for (const auto& connection : graph.getConnections())
        graph.removeConnection(connection, Graph::UpdateKind::none);
    for (const auto id : linkNodes)
        graph.removeNode(id, Graph::UpdateKind::none);
    linkNodes.clear();
    const auto connectAudio = [this](Graph::NodeID from, Graph::NodeID to)
    {
        for (int channel = 0; channel < 2; ++channel)
            graph.addConnection({ { from, channel }, { to, channel } }, Graph::UpdateKind::none);
    };
    auto previous = audioIn;
    for (const auto& node : layout.serial)
        if (const auto it = blocks.find(node.id); it != blocks.end())
        {
            connectAudio(previous, it->second.node);
            previous = it->second.node;
        }
    if (layout.chainLevelDb != 0.0f)
    {
        // The serial path's level into the output (e.g. off for a synth-only sound).
        if (const auto level = graph.addNode(std::make_unique<chainnodes::Gain>(layout.chainLevelDb), std::nullopt, Graph::UpdateKind::none))
        {
            linkNodes.push_back(level->nodeID);
            connectAudio(previous, level->nodeID);
            connectAudio(level->nodeID, audioOut);
        }
    }
    else
        connectAudio(previous, audioOut);
    const auto endpoint = [this](const juce::String& id, bool source) -> std::optional<Graph::NodeID>
    {
        if (id == chain::input) return audioIn;
        if (id == chain::keys) return midiIn;
        if (id == chain::output) return audioOut;
        if (const auto it = blocks.find(id); it != blocks.end())
            return it->second.node;
        juce::ignoreUnused(source);
        return std::nullopt;
    };
    for (const auto& link : layout.links)
    {
        const auto from = endpoint(link.from, true), to = endpoint(link.to, false);
        if (! from || ! to)
            continue;
        if (link.midi)
        {
            graph.addConnection({ { *from, Graph::midiChannelIndex }, { *to, Graph::midiChannelIndex } }, Graph::UpdateKind::none);
            continue;
        }
        const auto level = graph.addNode(std::make_unique<chainnodes::Gain>(link.gainDb), std::nullopt, Graph::UpdateKind::none);
        if (level == nullptr)
            continue;
        linkNodes.push_back(level->nodeID);
        // A link that would close a loop is refused by the graph.
        connectAudio(*from, level->nodeID);
        connectAudio(level->nodeID, *to);
    }
    graph.rebuild();
    currentLayout = layout;
}

chain::Layout RigChain::captureLayout(const chain::Layout& base)
{
    auto layout = base;
    for (auto* list : { &layout.serial, &layout.side })
        for (auto& node : *list)
            if (auto* slot = plugin(node.id))
            {
                const auto state = slot->saveState();
                if (! state.hasProperty("descriptionXml"))
                    continue;
                node.plugin = state;
                node.bypassed = slot->isBypassed();
                if (node.name.isEmpty())
                    if (const auto xml = juce::parseXML(state.getProperty("descriptionXml").toString()))
                        node.name = xml->getStringAttribute("name");
                blocks[node.id].loadedState = pluginKey(state); // it is what runs: no reload
            }
    return layout;
}

juce::ValueTree RigChain::currentSettings(const juce::ValueTree& settings)
{
    auto sound = SoundState::fromValueTree(settings);
    if (! sound)
        return {};
    sound->layout = captureLayout(sound->layout);
    return sound->toValueTree();
}

Vst3Slot* RigChain::plugin(const juce::String& blockId)
{
    const auto it = blocks.find(blockId);
    if (it == blocks.end())
        return nullptr;
    if (auto* node = graph.getNodeForId(it->second.node))
        if (auto* plugin = dynamic_cast<chainnodes::Plugin*>(node->getProcessor()))
            return &plugin->slot;
    return nullptr;
}

void RigChain::setBlockBypassed(const juce::String& blockId, bool bypassed)
{
    if (auto* slot = plugin(blockId))
        slot->setBypassed(bypassed);
    else if (const auto it = blocks.find(blockId); it != blocks.end())
        if (auto* node = graph.getNodeForId(it->second.node))
            if (auto* transpose = dynamic_cast<chainnodes::Transpose*>(node->getProcessor()))
                transpose->bypassed.store(bypassed);
    if (auto* node = currentLayout.find(blockId))
        node->bypassed = bypassed;
}

void RigChain::prepare(double sampleRate, int maximumBlockSize)
{
    maximumBlock = std::max(1, maximumBlockSize);
    namProcessor.setProcessingFormat(sampleRate, maximumBlock);
    namPedalProcessor.prepare(sampleRate, maximumBlock);
    namDryBuffer.setSize(1, maximumBlock, false, false, true);
    const juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(maximumBlock), 1 };
    eqProcessor.prepare(spec);
    graphicEq.prepare(spec);
    pitchShifter.prepare(sampleRate, maximumBlock);
    driveProcessor.prepare(spec);
    compressor.prepare(spec);
    wahPedal.prepare(spec);
    namBypassRamp.prepare(spec.sampleRate, false);
    namInputTrim.prepare(spec.sampleRate, .030, 0.f);
    irProcessor.prepare(spec);
    autoWahProcessor.prepare(spec);
    chorusProcessor.prepare(spec);
    delayProcessor.prepare(spec);
    reverbProcessor.prepare(spec);
    inputGain.reset(spec.sampleRate, 0.020);
    outputGain.reset(spec.sampleRate, 0.020);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputGainDb.load(std::memory_order_relaxed)));
    preparedRate = sampleRate;
    graph.setPlayConfigDetails(2, 2, sampleRate, maximumBlock);
    graph.prepareToPlay(sampleRate, maximumBlock);
}

int RigChain::latencySamples() const
{
    int total = pitchShifter.latencySamples() + irProcessor.latencySamples() + driveProcessor.latencySamples();
    for (const auto& [id, block] : blocks)
        if (block.kind == "vst3" && currentLayout.isSerial(id))
            if (auto* node = graph.getNodeForId(block.node))
                if (auto* plugin = dynamic_cast<chainnodes::Plugin*>(node->getProcessor()))
                    total += plugin->slot.latencySamples();
    return total;
}

void RigChain::applyEffectState(const SoundState& state)
{
    // Path loading is explicit via setNamPedal when selecting/restoring a pedal.
    namPedalProcessor.setControls(state.namPedalBypassed, state.namPedalInputDb, state.namPedalOutputDb);
    namInputTrim.set(juce::jlimit(-18.f, 18.f, state.namInputTrimDb));
    setInputGainDb(state.inputGainDb); setOutputGainDb(state.outputGainDb);
    setBassDb(state.bassDb); setMidDb(state.midDb); setTrebleDb(state.trebleDb);
    setNamBypassed(state.namBypassed); setIrBypassed(state.irBypassed);
    setDriveBypassed(state.driveBypassed); driveProcessor.setType(static_cast<nam::effects::DriveProcessor::DriveType>(juce::jlimit(0, 3, state.driveType))); driveProcessor.setDrive(state.driveAmount); driveProcessor.setTone(state.driveTone); driveProcessor.setWarmth(state.driveWarmth); driveProcessor.setPresence(state.drivePresence); driveProcessor.setSustain(state.driveSustain); driveProcessor.setBias(state.driveBias); driveProcessor.setLevel(state.driveLevel); driveProcessor.setMix(state.driveMix);
    setAutoWahBypassed(state.autoWahBypassed); autoWahProcessor.setSensitivity(state.autoWahSensitivity); autoWahProcessor.setAttack(state.autoWahAttackMs * 0.001f); autoWahProcessor.setRelease(state.autoWahReleaseMs * 0.001f); autoWahProcessor.setMinFrequency(state.autoWahMinimumHz); autoWahProcessor.setMaxFrequency(state.autoWahMaximumHz); autoWahProcessor.setResonance(state.autoWahQ); autoWahProcessor.setMix(state.autoWahMix); autoWahProcessor.setDirectionUp(state.autoWahUpward);
    setChorusBypassed(state.chorusBypassed); chorusProcessor.setRateHz(state.chorusRateHz); chorusProcessor.setDepth(juce::jlimit(0.0f, 1.0f, state.chorusDepthMs / 20.0f)); chorusProcessor.setCenterDelayMs(state.chorusDelayMs); chorusProcessor.setFeedback(state.chorusFeedback); chorusProcessor.setMix(state.chorusMix);
    setDelayBypassed(state.delayBypassed); delayProcessor.setTimeSeconds(state.delayTimeMs * 0.001f); delayProcessor.setFeedback(state.delayFeedback); delayProcessor.setMix(state.delayMix); delayProcessor.setLowCutHz(state.delayLowCutHz); delayProcessor.setHighCutHz(state.delayHighCutHz);
    setReverbBypassed(state.reverbBypassed); reverbProcessor.setSize(state.reverbSize); reverbProcessor.setDecay(state.reverbDecay); reverbProcessor.setDamping(state.reverbDamping); reverbProcessor.setPreDelay(state.reverbPreDelayMs); reverbProcessor.setMix(state.reverbMix); reverbProcessor.setLowCut(state.reverbLowCutHz); reverbProcessor.setHighCut(state.reverbHighCutHz);
    setGraphicEqBypassed(state.graphicEqBypassed);
    for (int band = 0; band < static_cast<int>(state.graphicEqDb.size()); ++band)
        setGraphicEqBandDb(band, state.graphicEqDb[static_cast<size_t>(band)]);
    setPitch(state.pitchBypassed, state.pitchSemitones, state.pitchMix, state.pitchFormantPreserved, state.pitchHighQuality, state.pitchAnalogOctave);
    setWahPedal(state.pedalWahBypassed, state.pedalWahPosition, state.pedalWahHeelHz, state.pedalWahToeHz, state.pedalWahQ, state.pedalWahMix);
    setCompressor(state.compressorBypassed, state.compressorThresholdDb, state.compressorRatio, state.compressorAttackMs,
                  state.compressorReleaseMs, state.compressorKneeDb, state.compressorMakeupDb, state.compressorMix);
}

float RigChain::processWithInputPeak(float* mono, float* left, float* right, int numSamples, juce::MidiBuffer* keyboard) noexcept
{
    float inPeak = 0.0f;
    if (numSamples > maximumBlock)
    {
        // Never allocate here; a block larger than announced is dropped.
        juce::FloatVectorOperations::clear(left, numSamples);
        juce::FloatVectorOperations::clear(right, numSamples);
        return inPeak;
    }

    inputGain.setTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDb.load(std::memory_order_relaxed)));
    for (int sample = 0; sample < numSamples; ++sample)
    {
        mono[sample] *= inputGain.getNextValue();
        inPeak = std::max(inPeak, std::abs(mono[sample]));
    }

    // The graph runs in stereo; the guitar feeds both sides of its input.
    juce::FloatVectorOperations::copy(left, mono, numSamples);
    juce::FloatVectorOperations::copy(right, mono, numSamples);
    float* stereoChannels[] { left, right };
    juce::AudioBuffer<float> stereoView(stereoChannels, 2, numSamples);
    graphMidi.clear();
    if (keyboard != nullptr)
        graphMidi.addEvents(*keyboard, 0, numSamples, 0);
    graph.processBlock(stereoView, graphMidi);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float gain = outputGain.getNextValue();
        left[sample] *= gain; right[sample] *= gain;
    }
    return inPeak;
}
