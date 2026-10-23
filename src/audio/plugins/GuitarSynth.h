#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>

/** Guitar to MIDI to synth for the live rig.

    A "tracker" VST3 (an audio-to-MIDI plugin such as MIDI Guitar, Transkr,
    MiGiC or TONIQ) listens to the dry guitar and produces MIDI notes; they
    drive an instrument VST3 (e.g. Vital) in real time. Notes from MIDI
    keyboards are merged in. The synth is mixed with the guitar, both with
    their own level.

    load / clear / state / instances: message thread. prepare: not the audio
    thread. process: audio thread, guarded by a try-lock so a plugin being
    swapped outputs silence instead of blocking. Third-party plugin code may
    still allocate. Editors must be closed before a plugin is replaced. */
class GuitarSynth final : public juce::MidiInputCallback, private juce::AsyncUpdater
{
public:
    GuitarSynth();
    ~GuitarSynth() override;

    enum class Slot { tracker, synth };

    juce::Result load(Slot, const juce::PluginDescription&, const juce::MemoryBlock* state = nullptr);
    void clear(Slot);
    /** Takes over an already created instance (used by load(); also lets tests inject plugins). */
    juce::Result adopt(Slot, std::unique_ptr<juce::AudioPluginInstance>, const juce::PluginDescription&);
    [[nodiscard]] juce::AudioPluginInstance* instance(Slot) noexcept;
    [[nodiscard]] juce::String name(Slot) const;
    [[nodiscard]] bool hasPlugin(Slot) const;

    [[nodiscard]] juce::ValueTree saveState();
    void restoreState(const juce::ValueTree&, juce::StringArray& errors);

    void prepare(double sampleRate, int maximumBlockSize);

    // Lock-free settings.
    void setEnabled(bool value) noexcept { enabled.store(value, std::memory_order_relaxed); }
    [[nodiscard]] bool isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }
    void setSynthLevelDb(float db) noexcept { synthLevelDb.store(juce::jlimit(-60.0f, 12.0f, db), std::memory_order_relaxed); }
    [[nodiscard]] float synthLevel() const noexcept { return synthLevelDb.load(std::memory_order_relaxed); }
    /** -60 dB and below mutes the guitar ("synth only"). */
    void setGuitarLevelDb(float db) noexcept { guitarLevelDb.store(juce::jlimit(-60.0f, 12.0f, db), std::memory_order_relaxed); }
    [[nodiscard]] float guitarLevel() const noexcept { return guitarLevelDb.load(std::memory_order_relaxed); }
    void setTranspose(int semitones) noexcept { transpose.store(juce::jlimit(-36, 36, semitones), std::memory_order_relaxed); }
    [[nodiscard]] int transposeSemitones() const noexcept { return transpose.load(std::memory_order_relaxed); }
    void setKeyboardsEnabled(bool value) noexcept { keyboards.store(value, std::memory_order_relaxed); }
    [[nodiscard]] bool keyboardsEnabled() const noexcept { return keyboards.load(std::memory_order_relaxed); }

    /** For the UI: the most recent note (-1 = none yet) and a counter of note-ons. */
    [[nodiscard]] int lastNote() const noexcept { return recentNote.load(std::memory_order_relaxed); }
    [[nodiscard]] int heldNotes() const noexcept { return held.load(std::memory_order_relaxed); }
    [[nodiscard]] juce::uint32 noteCount() const noexcept { return notesSeen.load(std::memory_order_relaxed); }

    /** Audio thread. dry: the guitar after the input gain. Scales left/right
        (the processed guitar) and adds the synth. Does nothing while disabled. */
    void process(const float* dry, float* left, float* right, int numSamples) noexcept;

    /** Every plugin in one VST3 file (message thread; run it in a child process).
        Trackers are those with an audio input, synths the instruments. */
    [[nodiscard]] static juce::Array<juce::PluginDescription> scanFile(const juce::File&);

private:
    struct Hosted
    {
        std::unique_ptr<juce::AudioPluginInstance> plugin;
        juce::PluginDescription description;
        juce::AudioBuffer<float> buffer;
    };

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;

    void prepareHosted(Hosted&, double rate, int block);
    static void release(std::unique_ptr<juce::AudioPluginInstance>&) noexcept;
    bool runPlugin(Hosted&, int numSamples) noexcept;

    juce::AudioPluginFormatManager formats; // outlives the instances
    mutable juce::CriticalSection lock;
    Hosted tracker, synth;
    juce::MidiBuffer midi, keyboardMidi, shifted;
    juce::MidiMessageCollector keyboardCollector;
    double sampleRate = 48000.0;
    int maximumBlock = 512;
    std::atomic<double> pendingRate { 48000.0 };
    std::atomic<int> pendingBlock { 512 };
    std::atomic<bool> enabled { false }, keyboards { true };
    std::atomic<float> synthLevelDb { 0.0f }, guitarLevelDb { 0.0f };
    std::atomic<int> transpose { 0 }, recentNote { -1 }, held { 0 };
    std::atomic<juce::uint32> notesSeen { 0 };
    juce::SmoothedValue<float> guitarGain { 1.0f }, synthGain { 1.0f };
    int heldCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarSynth)
};
