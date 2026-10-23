#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>

// One VST3 in the signal chain: an effect, an instrument (audio output only)
// or a MIDI generator such as a guitar-to-MIDI tracker (audio input, MIDI out).
// Build/link juce_audio_processors with JUCE_PLUGINHOST_VST3=1 (GUI format).
// load/clear/saveState/restoreState/scanFile/instance: message thread only.
// prepare: non-audio lifecycle thread; serialized with all plugin calls.
// Destroy editors BEFORE replacement/clear/destruction; stop callbacks before
// destruction. instance() is a borrowed pointer for editor creation only.
// The slot itself allocates nothing in process; third-party code may do so.
class Vst3Slot final : private juce::AsyncUpdater
{
public:
    Vst3Slot();
    ~Vst3Slot();
    void prepare(double sampleRate, int maximumBlockSize);
    /** midi: notes for the plugin; replaced by the notes it outputs (cleared
        if it outputs none or is bypassed). Without midi the plugin gets none. */
    void process(juce::AudioBuffer<float>&, juce::MidiBuffer* midi = nullptr) noexcept;
    juce::Result load(const juce::PluginDescription&);
    void clear();
    void setBypassed(bool);
    bool isBypassed() const;
    juce::String name() const;
    juce::AudioPluginInstance* instance();
    juce::ValueTree saveState();
    juce::Result restoreState(const juce::ValueTree&);
    int latencySamples() const;
    /** Every plugin type in one VST3 file (run it in a child process). */
    static juce::Array<juce::PluginDescription> scanFile(const juce::File&);

private:
    void handleAsyncUpdate() override;
    double pendingRate = 48000.0;
    int pendingBlock = 512;
    // Fixed ceiling, independent of device rate: 4 s at 48 kHz, ~1.5 MB stereo.
    static constexpr int maximumLatency = 192000;
    static constexpr int maximumSupportedBlock = 65536;
    juce::Result install(const juce::PluginDescription&, const juce::MemoryBlock*, bool);
    static juce::Result configureBuses(juce::AudioPluginInstance&);
    static bool onMessageThread();
    static void release(juce::AudioPluginInstance*) noexcept;
    void resetMixAndDelay() noexcept;

    mutable juce::CriticalSection mutex;
    juce::AudioPluginFormatManager formats; // Outlives the instance.
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;
    juce::AudioBuffer<float> work, dryDelay;
    juce::MidiBuffer midi;
    std::atomic<bool> bypassed { false };
    std::atomic<int> reportedLatency { 0 };
    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    bool prepared = false;
    bool faulted = false;
    int delayPosition = 0, delayValid = 0, currentLatency = 0;
    float wet = 0.0f, wetStep = 1.0f / 480.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Vst3Slot)
};
