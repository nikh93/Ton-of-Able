#include "PluginInstrument.h"

#include <array>

PluginInstrument::~PluginInstrument()
{
    const juce::ScopedLock scoped(lock);
    if (plugin != nullptr)
    {
        jassert(plugin->getActiveEditor() == nullptr); // editors must close first
        try { plugin->releaseResources(); } catch (...) {}
    }
}

juce::ValueTree PluginInstrument::settingsFor(const juce::PluginDescription& description, const juce::MemoryBlock& state)
{
    juce::ValueTree settings("Instrument");
    settings.setProperty("type", "vst3", nullptr);
    settings.setProperty("name", description.name, nullptr);
    settings.setProperty("stateBase64", state.toBase64Encoding(), nullptr);
    if (const auto xml = description.createXml())
        settings.addChild(juce::ValueTree::fromXml(*xml), -1, nullptr);
    return settings;
}

std::shared_ptr<PluginInstrument> PluginInstrument::fromSettings(const juce::ValueTree& settings, juce::String& error)
{
    if (settings.getProperty("type").toString() != "vst3")
    {
        error = "Kein VST3-Instrument.";
        return {};
    }
    juce::PluginDescription description;
    const auto xml = settings.getChild(0).createXml();
    if (xml == nullptr || ! description.loadFromXml(*xml))
    {
        error = "Instrument-Beschreibung fehlt.";
        return {};
    }
    std::shared_ptr<PluginInstrument> instrument(new PluginInstrument());
   #if JUCE_PLUGINHOST_VST3
    instrument->formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
   #endif
    try
    {
        instrument->plugin = instrument->formats.createPluginInstance(description, 48000.0, 512, error);
    }
    catch (...)
    {
        error = "Instrument konnte nicht erstellt werden: " + description.name;
    }
    if (instrument->plugin == nullptr)
    {
        if (error.isEmpty())
            error = "Instrument konnte nicht erstellt werden: " + description.name;
        return {};
    }
    juce::MemoryBlock state;
    if (state.fromBase64Encoding(settings.getProperty("stateBase64").toString()) && state.getSize() > 0)
        instrument->plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    instrument->description = description;
    return instrument;
}

juce::Array<juce::PluginDescription> PluginInstrument::scanFile(const juce::File& file)
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
            if (description->isInstrument)
                result.add(*description);
    }
    catch (...) {}
   #else
    juce::ignoreUnused(file);
   #endif
    return result;
}

void PluginInstrument::prepare(double sampleRate, int maximumBlockSize)
{
    const juce::ScopedLock scoped(lock);
    maximumBlock = std::max(1, maximumBlockSize);
    if (plugin == nullptr)
        return;
    plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
    plugin->setRateAndBufferSizeDetails(sampleRate, maximumBlock);
    plugin->prepareToPlay(sampleRate, maximumBlock);
    const auto channels = std::max({ 2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels() });
    buffer.setSize(channels, maximumBlock, false, true, false);
    midi.ensureSize(16384);
}

void PluginInstrument::render(const daw::DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept
{
    const juce::ScopedTryLock scoped(lock);
    if (! scoped.isLocked() || plugin == nullptr || numSamples > maximumBlock)
    {
        juce::FloatVectorOperations::clear(left, numSamples);
        juce::FloatVectorOperations::clear(right, numSamples);
        return;
    }
    daw::DrumEvent previews[16];
    const auto previewCount = takePreviews(previews, 16);
    const auto rightChannel = plugin->getTotalNumOutputChannels() > 1 ? 1 : 0;
    std::array<float*, 64> channels {};
    const auto numChannels = std::min(buffer.getNumChannels(), static_cast<int>(channels.size()));

    // Split the block at every event position and send each event at offset
    // 0 of its sub-block: hits stay sample-accurate even with plugins that
    // handle MIDI only at block starts (JDrummer does).
    int eventIndex = 0;
    for (int position = 0; position < numSamples;)
    {
        midi.clear();
        if (position == 0)
            for (int index = 0; index < previewCount; ++index)
                midi.addEvent(juce::MidiMessage::noteOn(10, previews[index].note, previews[index].velocity), 0);
        for (; eventIndex < numEvents && events[eventIndex].offset <= position; ++eventIndex)
        {
            const auto& event = events[eventIndex];
            midi.addEvent(event.velocity > 0 ? juce::MidiMessage::noteOn(10, event.note, event.velocity)
                                             : juce::MidiMessage::noteOff(10, event.note), 0);
        }
        const auto next = eventIndex < numEvents ? juce::jlimit(position + 1, numSamples, events[eventIndex].offset) : numSamples;
        const auto length = next - position;
        for (int channel = 0; channel < numChannels; ++channel)
            channels[static_cast<size_t>(channel)] = buffer.getWritePointer(channel);
        juce::AudioBuffer<float> view(channels.data(), numChannels, length);
        view.clear();
        try
        {
            plugin->processBlock(view, midi);
        }
        catch (...)
        {
            view.clear();
        }
        juce::FloatVectorOperations::copy(left + position, view.getReadPointer(0), length);
        juce::FloatVectorOperations::copy(right + position, view.getReadPointer(rightChannel), length);
        position = next;
    }
}

juce::ValueTree PluginInstrument::currentSettings()
{
    juce::MemoryBlock state;
    {
        const juce::ScopedLock scoped(lock);
        if (plugin != nullptr)
            plugin->getStateInformation(state);
    }
    return settingsFor(description, state);
}

std::shared_ptr<daw::TrackInstrument> PluginInstrument::createOfflineCopy() const
{
    juce::String error;
    return fromSettings(const_cast<PluginInstrument*>(this)->currentSettings(), error);
}
