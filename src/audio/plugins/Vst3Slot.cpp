#include "Vst3Slot.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

bool Vst3Slot::onMessageThread()
{
    const auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    return manager != nullptr && manager->isThisTheMessageThread();
}

Vst3Slot::Vst3Slot()
{
   #if JUCE_PLUGINHOST_VST3
    // JUCE 9 GUI extension: preserves native plugin editors.
    formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
   #endif
    prepare(sampleRate, maximumBlockSize);
}

Vst3Slot::~Vst3Slot()
{
    cancelPendingUpdate();
    const juce::ScopedLock lock(mutex);
    jassert(plugin == nullptr || plugin->getActiveEditor() == nullptr);
    release(plugin.get());
    plugin.reset();
}

void Vst3Slot::release(juce::AudioPluginInstance* p) noexcept
{
    if (p != nullptr)
        try { p->releaseResources(); } catch (...) {}
}

void Vst3Slot::resetMixAndDelay() noexcept
{
    delayPosition = delayValid = 0;
    wet = 0.0f;
    // Unwritten ring entries are never read, so no large clear is necessary.
}

void Vst3Slot::prepare(double rate, int blockSize)
{
    const juce::ScopedLock lock(mutex);
    prepared = false;
    if (!onMessageThread())
    {
        // Drivers may invoke device lifecycle callbacks from their own thread.
        // Never enter VST3 setup there while holding a lock needed by the UI.
        pendingRate=rate; pendingBlock=blockSize;
        triggerAsyncUpdate();
        return;
    }
    cancelPendingUpdate();
    release(plugin.get());
    reportedLatency.store(0, std::memory_order_relaxed);
    resetMixAndDelay();
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 768000.0
        || blockSize < 1 || blockSize > maximumSupportedBlock)
    {
        jassertfalse;
        return; // Unsupported format: process leaves input untouched.
    }
    sampleRate = rate;
    maximumBlockSize = blockSize;
    work.setSize(2, blockSize);
    dryDelay.setSize(2, maximumLatency + 1);
    midi.ensureSize(4096);
    wetStep = static_cast<float>(1.0 / (0.010 * rate));
    faulted = false;
    try
    {
        if (plugin != nullptr)
        {
            plugin->setRateAndBufferSizeDetails(rate, blockSize);
            plugin->prepareToPlay(rate, blockSize);
            currentLatency = plugin->getLatencySamples();
            if (currentLatency < 0 || currentLatency > maximumLatency)
                throw std::runtime_error("Plugin latency exceeds slot capacity");
            reportedLatency.store(currentLatency, std::memory_order_relaxed);
        }
        prepared = true;
    }
    catch (...)
    {
        release(plugin.get());
        faulted = true;
    }
}

void Vst3Slot::handleAsyncUpdate()
{
    const juce::ScopedLock lock(mutex);
    prepare(pendingRate,pendingBlock);
}

juce::Result Vst3Slot::configureBuses(juce::AudioPluginInstance& p)
{
    const auto hasInput = p.getBusCount(true) > 0, hasOutput = p.getBusCount(false) > 0;
    if (! hasInput && ! hasOutput)
        return juce::Result::ok(); // pure MIDI effect
    // Prefer stereo, then mono-to-stereo, then mono, then stereo-to-mono;
    // instruments have no input, MIDI trackers may have no output.
    for (const auto channels : { std::pair{2, 2}, std::pair{1, 2}, std::pair{1, 1}, std::pair{2, 1},
                                 std::pair{0, 2}, std::pair{0, 1}, std::pair{2, 0}, std::pair{1, 0} })
    {
        if ((channels.first > 0) != hasInput || (channels.second > 0) != hasOutput)
            continue;
        auto layout = p.getBusesLayout();
        for (auto& bus : layout.inputBuses) bus = juce::AudioChannelSet::disabled();
        for (auto& bus : layout.outputBuses) bus = juce::AudioChannelSet::disabled();
        if (hasInput)
            layout.inputBuses.getReference(0) = juce::AudioChannelSet::canonicalChannelSet(channels.first);
        if (hasOutput)
            layout.outputBuses.getReference(0) = juce::AudioChannelSet::canonicalChannelSet(channels.second);
        if (p.checkBusesLayoutSupported(layout) && p.setBusesLayout(layout)
            && p.getTotalNumInputChannels() == channels.first
            && p.getTotalNumOutputChannels() == channels.second)
            return juce::Result::ok();
    }
    return juce::Result::fail("Unsupported buses: requires 1/2-channel main buses with auxiliaries disabled");
}

juce::Result Vst3Slot::install(const juce::PluginDescription& desc,
                              const juce::MemoryBlock* state, bool bypass)
{
    if (!onMessageThread())
        return juce::Result::fail("VST3 loading requires the message thread");
    if (desc.pluginFormatName != "VST3")
        return juce::Result::fail("Only VST3 plugins are supported");
    if (!juce::File::isAbsolutePath(desc.fileOrIdentifier)
        || !juce::File(desc.fileOrIdentifier).exists()) // .vst3 may be a directory.
        return juce::Result::fail("VST3 missing: " + desc.fileOrIdentifier);
    const juce::ScopedLock lock(mutex);
    if (!prepared)
        return juce::Result::fail("Prepare the slot with a supported audio format first");
    if (plugin != nullptr && plugin->getActiveEditor() != nullptr)
        return juce::Result::fail("Close the current plugin editor before replacing the slot");
    std::unique_ptr<juce::AudioPluginInstance> candidate;
    try
    {
        juce::String error;
        candidate = formats.createPluginInstance(desc, sampleRate, maximumBlockSize, error);
        if (!candidate)
            return juce::Result::fail(error.isNotEmpty() ? error : "VST3 hosting unavailable or plugin creation failed");
        auto configured = configureBuses(*candidate);
        if (configured.failed())
        {
            release(candidate.get());
            return configured;
        }
        if (state != nullptr && state->getSize() > 0)
            candidate->setStateInformation(state->getData(), static_cast<int>(state->getSize()));
        // State restoration can re-enable buses; enforce the host layout again.
        configured = configureBuses(*candidate);
        if (configured.failed())
        {
            release(candidate.get());
            return configured;
        }
        candidate->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        candidate->setRateAndBufferSizeDetails(sampleRate, maximumBlockSize);
        candidate->prepareToPlay(sampleRate, maximumBlockSize);
        const int latency = candidate->getLatencySamples();
        if (latency < 0 || latency > maximumLatency)
        {
            release(candidate.get());
            return juce::Result::fail("VST3 latency exceeds 192000 samples");
        }
        // Commit only after creation, state, bus validation and preparation succeed.
        release(plugin.get());
        plugin = std::move(candidate);
        description = desc;
        bypassed.store(bypass, std::memory_order_relaxed);
        faulted = false;
        currentLatency = latency;
        reportedLatency.store(latency, std::memory_order_relaxed);
        resetMixAndDelay();
        return juce::Result::ok();
    }
    catch (const std::exception& e)
    {
        release(candidate.get());
        return juce::Result::fail("VST3 load failed: " + juce::String(e.what()));
    }
    catch (...)
    {
        release(candidate.get());
        return juce::Result::fail("VST3 load failed with an unknown exception");
    }
}

juce::Result Vst3Slot::load(const juce::PluginDescription& desc)
{
    return install(desc, nullptr, isBypassed());
}

void Vst3Slot::clear()
{
    if (!onMessageThread()) { jassertfalse; return; }
    const juce::ScopedLock lock(mutex);
    if (plugin != nullptr && plugin->getActiveEditor() != nullptr) { jassertfalse; return; }
    release(plugin.get());
    plugin.reset();
    description = {};
    faulted = false;
    currentLatency = 0;
    reportedLatency.store(0, std::memory_order_relaxed);
    resetMixAndDelay();
}

void Vst3Slot::setBypassed(bool value) { bypassed.store(value, std::memory_order_relaxed); }
bool Vst3Slot::isBypassed() const { return bypassed.load(std::memory_order_relaxed); }
int Vst3Slot::latencySamples() const { return reportedLatency.load(std::memory_order_relaxed); }

juce::String Vst3Slot::name() const
{
    const juce::ScopedLock lock(mutex);
    return plugin != nullptr ? description.name + (faulted ? " (Fehler: Dry-Fallback)" : "") : juce::String{};
}

juce::AudioPluginInstance* Vst3Slot::instance()
{
    if (!onMessageThread()) { jassertfalse; return nullptr; }
    const juce::ScopedLock lock(mutex);
    return plugin.get();
}

void Vst3Slot::process(juce::AudioBuffer<float>& audio, juce::MidiBuffer* notes) noexcept
{
    auto& events = notes != nullptr ? *notes : midi;
    if (notes == nullptr)
        midi.clear();
    const auto dropMidi = [&events] { events.clear(); };
    const juce::ScopedTryLock lock(mutex);
    if (!lock.isLocked()) return dropMidi(); // Emergency immediate dry; no UI/load waiting.
    const int count = audio.getNumSamples(), channels = audio.getNumChannels();
    if (!prepared || faulted || !plugin || count <= 0 || count > maximumBlockSize
        || (channels != 1 && channels != 2)) return dropMidi();
    const juce::ScopedTryLock pluginLock(plugin->getCallbackLock());
    if (!pluginLock.isLocked()) return dropMidi();
    const int ins = plugin->getTotalNumInputChannels(), outs = plugin->getTotalNumOutputChannels();
    const int latency = plugin->getLatencySamples();
    if (ins < 0 || ins > 2 || outs < 0 || outs > 2 || latency < 0 || latency > maximumLatency)
    {
        faulted = true;
        reportedLatency.store(0, std::memory_order_relaxed);
        return;
    }
    if (latency != currentLatency)
    {
        currentLatency = latency;
        reportedLatency.store(latency, std::memory_order_relaxed);
        resetMixAndDelay();
    }
    auto* left = work.getWritePointer(0);
    auto* right = work.getWritePointer(1);
    const auto* inLeft = audio.getReadPointer(0);
    const auto* inRight = audio.getReadPointer(channels == 2 ? 1 : 0);
    for (int i = 0; i < count; ++i)
    {
        // Instruments (no input) start from silence.
        left[i] = ins == 0 ? 0.0f : ins == 1 ? 0.5f * inLeft[i] + 0.5f * inRight[i] : inLeft[i];
        right[i] = ins == 2 ? inRight[i] : 0.0f;
    }
    // External-data views with <=2 channels use JUCE's inline channel pointer
    // storage. Never resize owned buffers or allocate on this path.
    float* pointers[] { left, right };
    juce::AudioBuffer<float> block(pointers, std::max({ ins, outs, 1 }), count);
    try
    {
        // Run wet even when bypassed so tails and latency stay continuous.
        plugin->processBlock(block, events);
    }
    catch (...)
    {
        faulted = true;
        reportedLatency.store(0, std::memory_order_relaxed);
        return dropMidi(); // Original caller buffer is still intact.
    }
    if (isBypassed() || ! plugin->producesMidi())
        dropMidi();
    if (outs == 0)
        return; // a listener (e.g. guitar-to-MIDI): the audio passes unchanged
    for (int i = 0; i < count; ++i)
        if (!std::isfinite(left[i]) || (outs == 2 && !std::isfinite(right[i])))
        {
            faulted = true;
            reportedLatency.store(0, std::memory_order_relaxed);
            return;
        }
    const float target = isBypassed() ? 0.0f : 1.0f;
    auto* delayLeft = dryDelay.getWritePointer(0);
    auto* delayRight = dryDelay.getWritePointer(1);
    auto* outputLeft = audio.getWritePointer(0);
    auto* outputRight = channels == 2 ? audio.getWritePointer(1) : nullptr;
    constexpr int ringSize = maximumLatency + 1;
    for (int i = 0; i < count; ++i)
    {
        delayLeft[delayPosition] = inLeft[i];
        delayRight[delayPosition] = inRight[i];
        const int read = (delayPosition + ringSize - currentLatency) % ringSize;
        const float dl = delayValid >= currentLatency ? delayLeft[read] : 0.0f;
        const float dr = delayValid >= currentLatency ? delayRight[read] : 0.0f;
        delayPosition = (delayPosition + 1) % ringSize;
        delayValid = std::min(delayValid + 1, maximumLatency);
        wet = target > wet ? std::min(target, wet + wetStep) : std::max(target, wet - wetStep);
        const float wl = left[i], wr = outs == 2 ? right[i] : wl;
        // Double intermediates also handle finite-but-extreme plugin output:
        // float subtraction/addition could otherwise overflow during mixing.
        const auto blend = [this](float dry, double effect) noexcept
        {
            const double value = (1.0 - static_cast<double>(wet)) * dry + static_cast<double>(wet) * effect;
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
            {
                faulted = true;
                reportedLatency.store(0, std::memory_order_relaxed);
                return std::isfinite(dry) ? dry : 0.0f;
            }
            return static_cast<float>(value);
        };
        if (channels == 1)
            outputLeft[i] = blend(dl, 0.5 * static_cast<double>(wl) + 0.5 * static_cast<double>(wr));
        else
        {
            outputLeft[i] = blend(dl, wl);
            outputRight[i] = blend(dr, wr);
        }
    }
}

juce::ValueTree Vst3Slot::saveState()
{
    if (!onMessageThread()) { jassertfalse; return {}; }
    const juce::ScopedLock lock(mutex);
    juce::ValueTree tree("Vst3Slot");
    tree.setProperty("version", 1, nullptr);
    tree.setProperty("bypassed", isBypassed(), nullptr);
    if (plugin != nullptr)
    {
        juce::MemoryBlock state;
        try { plugin->getStateInformation(state); }
        catch (...) { return {}; } // Invalid tree signals a failed save, not empty state.
        tree.setProperty("descriptionXml", description.createXml()->toString(), nullptr);
        tree.setProperty("stateBase64", state.toBase64Encoding(), nullptr);
    }
    return tree;
}

juce::Result Vst3Slot::restoreState(const juce::ValueTree& tree)
{
    if (!onMessageThread())
        return juce::Result::fail("State restoration requires the message thread");
    if (!tree.hasType("Vst3Slot") || static_cast<int>(tree.getProperty("version", 0)) != 1)
        return juce::Result::fail("Invalid Vst3Slot state/version");
    const bool bypass = static_cast<bool>(tree.getProperty("bypassed", false));
    if (!tree.hasProperty("descriptionXml"))
    {
        const juce::ScopedLock lock(mutex);
        if (plugin != nullptr && plugin->getActiveEditor() != nullptr)
            return juce::Result::fail("Close the plugin editor before clearing the slot");
        clear();
        setBypassed(bypass);
        return juce::Result::ok();
    }
    auto xml = juce::parseXML(tree["descriptionXml"].toString());
    juce::PluginDescription desc;
    if (xml == nullptr || !desc.loadFromXml(*xml) || !tree.hasProperty("stateBase64"))
        return juce::Result::fail("Invalid plugin description/state");
    juce::MemoryBlock state;
    const auto encoded = tree["stateBase64"].toString();
    // An empty state is a freshly added plugin with its default settings.
    if ((encoded.isNotEmpty() && !state.fromBase64Encoding(encoded))
        || state.getSize() > static_cast<size_t>(std::numeric_limits<int>::max()))
        return juce::Result::fail("Invalid or oversized plugin state");
    // Failure, including a missing bundle, leaves the old slot untouched.
    return install(desc, &state, bypass);
}

juce::Array<juce::PluginDescription> Vst3Slot::scanFile(const juce::File& file)
{
    juce::Array<juce::PluginDescription> result;
    if (!onMessageThread()) { jassertfalse; return result; }
   #if JUCE_PLUGINHOST_VST3
    if (!file.exists() || !file.hasFileExtension("vst3")) return result;
    try
    {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions, file.getFullPathName());
        for (const auto* desc : descriptions)
            result.add(*desc); // Preserve every class ID in multi-type bundles.
        // Bus negotiation occurs on load; description counts may include
        // optional sidechains and are not enough to reject surround/aux layouts.
    }
    catch (...) {} // Scan failure returns an empty list.
   #else
    juce::ignoreUnused(file);
   #endif
    return result;
}
