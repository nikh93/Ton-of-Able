#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "daw/Drums.h"

/** A VST3 instrument as the sound source of a drum track (e.g. JDrummer).
    Pattern hits are sent as General MIDI percussion on channel 10.

    Creation, state and editor: message thread. render(): audio thread,
    guarded by a try-lock so a reconfiguring plugin outputs silence instead
    of blocking. Third-party plugin code may still allocate. */
class PluginInstrument final : public daw::TrackInstrument
{
public:
    ~PluginInstrument() override;

    /** Message thread. Settings: Instrument { type="vst3", stateBase64, PLUGIN }. */
    [[nodiscard]] static std::shared_ptr<PluginInstrument> fromSettings(const juce::ValueTree& settings, juce::String& error);
    [[nodiscard]] static juce::ValueTree settingsFor(const juce::PluginDescription&, const juce::MemoryBlock& state = {});

    void prepare(double sampleRate, int maximumBlockSize) override;
    void render(const daw::DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept override;
    [[nodiscard]] std::shared_ptr<daw::TrackInstrument> createOfflineCopy() const override;
    [[nodiscard]] juce::String name() const override { return description.name; }
    [[nodiscard]] juce::ValueTree currentSettings() override;

    /** For the plugin editor window (message thread). */
    [[nodiscard]] juce::AudioPluginInstance* instance() noexcept { return plugin.get(); }

private:
    PluginInstrument() = default;

    juce::AudioPluginFormatManager formats;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;
    juce::CriticalSection lock;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    int maximumBlock = 0;
};
