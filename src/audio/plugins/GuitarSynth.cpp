#include "GuitarSynth.h"

#include <array>

namespace
{
juce::String stateOf(juce::AudioPluginInstance* plugin)
{
    juce::MemoryBlock state;
    if (plugin != nullptr)
        try { plugin->getStateInformation(state); } catch (...) {}
    return state.toBase64Encoding();
}

bool isNoteOn(const juce::MidiMessage& message) { return message.isNoteOn(); }
} // namespace

GuitarSynth::GuitarSynth()
{
   #if JUCE_PLUGINHOST_VST3
    formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
   #endif
    midi.ensureSize(8192);
    keyboardMidi.ensureSize(4096);
    shifted.ensureSize(8192);
}

GuitarSynth::~GuitarSynth()
{
    cancelPendingUpdate();
    const juce::ScopedLock scoped(lock);
    release(tracker.plugin);
    release(synth.plugin);
}

void GuitarSynth::release(std::unique_ptr<juce::AudioPluginInstance>& plugin) noexcept
{
    if (plugin == nullptr)
        return;
    jassert(plugin->getActiveEditor() == nullptr); // editors must close first
    try { plugin->releaseResources(); } catch (...) {}
}

juce::Array<juce::PluginDescription> GuitarSynth::scanFile(const juce::File& file)
{
    juce::Array<juce::PluginDescription> result;
   #if JUCE_PLUGINHOST_VST3
    if (! file.exists() || ! file.hasFileExtension("vst3"))
        return result;
    try
    {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions, file.getFullPathName());
        for (const auto* description : descriptions)
            result.add(*description);
    }
    catch (...) {}
   #else
    juce::ignoreUnused(file);
   #endif
    return result;
}

juce::Result GuitarSynth::adopt(Slot slot, std::unique_ptr<juce::AudioPluginInstance> plugin, const juce::PluginDescription& description)
{
    if (plugin == nullptr)
        return juce::Result::fail("Kein Plugin.");
    auto& hosted = slot == Slot::tracker ? tracker : synth;
    // Prepare the new instance outside the lock, then swap it in.
    Hosted next;
    next.plugin = std::move(plugin);
    next.description = description;
    prepareHosted(next, sampleRate, maximumBlock);
    {
        const juce::ScopedLock scoped(lock);
        std::swap(hosted.plugin, next.plugin);
        std::swap(hosted.description, next.description);
        std::swap(hosted.buffer, next.buffer);
    }
    release(next.plugin); // the previous instance
    next.plugin.reset();
    return juce::Result::ok();
}

void GuitarSynth::prepareHosted(Hosted& hosted, double rate, int block)
{
    if (hosted.plugin == nullptr)
        return;
    try
    {
        hosted.plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        hosted.plugin->setRateAndBufferSizeDetails(rate, block);
        hosted.plugin->prepareToPlay(rate, block);
    }
    catch (...) {}
    const auto channels = std::max({ 2, hosted.plugin->getTotalNumInputChannels(), hosted.plugin->getTotalNumOutputChannels() });
    hosted.buffer.setSize(channels, block, false, true, false);
}

juce::Result GuitarSynth::load(Slot slot, const juce::PluginDescription& description, const juce::MemoryBlock* state)
{
    if (slot == Slot::synth && ! description.isInstrument)
        return juce::Result::fail(description.name + " ist kein Instrument.");
    if (slot == Slot::tracker && description.numInputChannels <= 0)
        return juce::Result::fail(description.name + " hat keinen Audio-Eingang und kann die Gitarre nicht hören.");
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    try
    {
        plugin = formats.createPluginInstance(description, sampleRate, maximumBlock, error);
    }
    catch (...)
    {
        error = "Plugin konnte nicht erstellt werden: " + description.name;
    }
    if (plugin == nullptr)
        return juce::Result::fail(error.isNotEmpty() ? error : "Plugin konnte nicht erstellt werden: " + description.name);
    try
    {
        plugin->enableAllBuses();
        if (state != nullptr && state->getSize() > 0)
            plugin->setStateInformation(state->getData(), static_cast<int>(state->getSize()));
    }
    catch (...)
    {
        return juce::Result::fail("Plugin-Zustand konnte nicht geladen werden: " + description.name);
    }
    return adopt(slot, std::move(plugin), description);
}

void GuitarSynth::clear(Slot slot)
{
    auto& hosted = slot == Slot::tracker ? tracker : synth;
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock scoped(lock);
        std::swap(old, hosted.plugin);
        hosted.description = {};
    }
    release(old);
}

juce::AudioPluginInstance* GuitarSynth::instance(Slot slot) noexcept
{
    return (slot == Slot::tracker ? tracker : synth).plugin.get();
}

juce::String GuitarSynth::name(Slot slot) const
{
    const juce::ScopedLock scoped(lock);
    return (slot == Slot::tracker ? tracker : synth).description.name;
}

bool GuitarSynth::hasPlugin(Slot slot) const
{
    const juce::ScopedLock scoped(lock);
    return (slot == Slot::tracker ? tracker : synth).plugin != nullptr;
}

juce::ValueTree GuitarSynth::saveState()
{
    juce::ValueTree state("GuitarSynth");
    state.setProperty("enabled", isEnabled(), nullptr);
    state.setProperty("synthDb", synthLevel(), nullptr);
    state.setProperty("guitarDb", guitarLevel(), nullptr);
    state.setProperty("transpose", transposeSemitones(), nullptr);
    state.setProperty("keyboards", keyboardsEnabled(), nullptr);
    for (auto* hosted : { &tracker, &synth })
    {
        if (hosted->plugin == nullptr)
            continue;
        juce::ValueTree node(hosted == &tracker ? "Tracker" : "Synth");
        node.setProperty("stateBase64", stateOf(hosted->plugin.get()), nullptr);
        if (const auto xml = hosted->description.createXml())
            node.addChild(juce::ValueTree::fromXml(*xml), -1, nullptr);
        state.addChild(node, -1, nullptr);
    }
    return state;
}

void GuitarSynth::restoreState(const juce::ValueTree& state, juce::StringArray& errors)
{
    if (! state.hasType("GuitarSynth"))
        return;
    setSynthLevelDb(state.getProperty("synthDb", 0.0f));
    setGuitarLevelDb(state.getProperty("guitarDb", 0.0f));
    setTranspose(state.getProperty("transpose", 0));
    setKeyboardsEnabled(state.getProperty("keyboards", true));
    for (const auto slot : { Slot::tracker, Slot::synth })
    {
        const auto node = state.getChildWithName(slot == Slot::tracker ? "Tracker" : "Synth");
        if (! node.isValid())
            continue;
        juce::PluginDescription description;
        const auto xml = node.getChild(0).createXml();
        if (xml == nullptr || ! description.loadFromXml(*xml))
            continue;
        juce::MemoryBlock data;
        data.fromBase64Encoding(node.getProperty("stateBase64").toString());
        if (const auto result = load(slot, description, &data); result.failed())
            errors.add(result.getErrorMessage());
    }
    setEnabled(state.getProperty("enabled", false));
}

void GuitarSynth::prepare(double rate, int block)
{
    if (! juce::MessageManager::existsAndIsCurrentThread())
    {
        // Drivers may start devices from their own thread: set plugins up on the message thread.
        pendingRate.store(rate);
        pendingBlock.store(block);
        triggerAsyncUpdate();
        return;
    }
    const juce::ScopedLock scoped(lock);
    sampleRate = rate;
    maximumBlock = std::max(1, block);
    for (auto* hosted : { &tracker, &synth })
    {
        if (hosted->plugin != nullptr)
            try { hosted->plugin->releaseResources(); } catch (...) {}
        prepareHosted(*hosted, sampleRate, maximumBlock);
    }
    keyboardCollector.reset(sampleRate);
    guitarGain.reset(sampleRate, 0.02);
    synthGain.reset(sampleRate, 0.02);
    heldCount = 0;
    held.store(0);
}

void GuitarSynth::handleAsyncUpdate()
{
    prepare(pendingRate.load(), pendingBlock.load());
}

void GuitarSynth::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (keyboards.load(std::memory_order_relaxed) && ! message.isActiveSense() && ! message.isMidiClock())
        keyboardCollector.addMessageToQueue(message);
}

bool GuitarSynth::runPlugin(Hosted& hosted, int numSamples) noexcept
{
    std::array<float*, 64> channels {};
    const auto count = std::min(hosted.buffer.getNumChannels(), static_cast<int>(channels.size()));
    for (int channel = 0; channel < count; ++channel)
        channels[static_cast<size_t>(channel)] = hosted.buffer.getWritePointer(channel);
    juce::AudioBuffer<float> view(channels.data(), count, numSamples);
    try
    {
        hosted.plugin->processBlock(view, midi);
        return true;
    }
    catch (...)
    {
        hosted.buffer.clear();
        midi.clear();
        return false;
    }
}

void GuitarSynth::process(const float* dry, float* left, float* right, int numSamples) noexcept
{
    if (! enabled.load(std::memory_order_relaxed))
        return;
    const juce::ScopedTryLock scoped(lock);
    if (! scoped.isLocked() || numSamples > maximumBlock || numSamples > tracker.buffer.getNumSamples() && tracker.plugin != nullptr
        || numSamples > synth.buffer.getNumSamples() && synth.plugin != nullptr)
        return;

    // 1. The tracker hears the dry guitar and writes the notes it detects into midi.
    midi.clear();
    if (tracker.plugin != nullptr)
    {
        const auto inputs = std::max(1, tracker.plugin->getTotalNumInputChannels());
        for (int channel = 0; channel < tracker.buffer.getNumChannels(); ++channel)
        {
            if (channel < inputs)
                juce::FloatVectorOperations::copy(tracker.buffer.getWritePointer(channel), dry, numSamples);
            else
                juce::FloatVectorOperations::clear(tracker.buffer.getWritePointer(channel), numSamples);
        }
        runPlugin(tracker, numSamples);
    }

    // 2. Notes from MIDI keyboards.
    keyboardMidi.clear();
    keyboardCollector.removeNextBlockOfMessages(keyboardMidi, numSamples);
    midi.addEvents(keyboardMidi, 0, numSamples, 0);

    // 3. Transpose and watch the notes for the UI.
    const auto shift = transpose.load(std::memory_order_relaxed);
    shifted.clear();
    for (const auto metadata : midi)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOnOrOff() || message.isAftertouch())
        {
            const auto note = message.getNoteNumber() + shift;
            if (note < 0 || note > 127)
                continue;
            message.setNoteNumber(note);
        }
        if (isNoteOn(message))
        {
            recentNote.store(message.getNoteNumber(), std::memory_order_relaxed);
            notesSeen.fetch_add(1, std::memory_order_relaxed);
            ++heldCount;
        }
        else if (message.isNoteOff())
            heldCount = std::max(0, heldCount - 1);
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            heldCount = 0;
        shifted.addEvent(message, metadata.samplePosition);
    }
    held.store(heldCount, std::memory_order_relaxed);
    midi.swapWith(shifted);

    // 4. The synth plays them.
    const auto guitarDb = guitarLevelDb.load(std::memory_order_relaxed);
    guitarGain.setTargetValue(guitarDb <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain(guitarDb));
    synthGain.setTargetValue(juce::Decibels::decibelsToGain(synthLevelDb.load(std::memory_order_relaxed)));
    const float* synthLeft = nullptr;
    const float* synthRight = nullptr;
    if (synth.plugin != nullptr)
    {
        synth.buffer.clear(0, numSamples);
        if (runPlugin(synth, numSamples))
        {
            synthLeft = synth.buffer.getReadPointer(0);
            synthRight = synth.buffer.getReadPointer(synth.plugin->getTotalNumOutputChannels() > 1 ? 1 : 0);
        }
    }
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto guitar = guitarGain.getNextValue();
        const auto level = synthGain.getNextValue();
        left[sample] = left[sample] * guitar + (synthLeft != nullptr ? synthLeft[sample] * level : 0.0f);
        right[sample] = right[sample] * guitar + (synthRight != nullptr ? synthRight[sample] * level : 0.0f);
    }
}
