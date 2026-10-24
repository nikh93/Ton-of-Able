#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <juce_data_structures/juce_data_structures.h>

namespace daw
{
// Drum machine: step patterns on drum tracks, played by a TrackInstrument -
// the built-in DrumSynth or any VST3 instrument (General MIDI drum notes, so
// SoundFont drum plugins such as JDrummer work out of the box).

constexpr int numDrumLanes = 12;

struct DrumLane
{
    const char* name;
    int note;   // General MIDI percussion key
};
[[nodiscard]] const std::array<DrumLane, numDrumLanes>& drumLanes();

struct DrumPattern
{
    static constexpr int maxSteps = 64;

    juce::String name;
    int steps = 16;          // sixteenth notes
    float swing = 0.0f;      // 0 ... 0.6: delays every second sixteenth
    std::array<std::array<juce::uint8, maxSteps>, numDrumLanes> velocity {}; // 0 = off

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] juce::ValueTree toValueTree() const;
    [[nodiscard]] static DrumPattern fromValueTree(const juce::ValueTree&);
};

/** Eight starter patterns: rock, fill, half-time, four-on-the-floor, metal,
    shuffle, funk and an empty one. */
[[nodiscard]] std::vector<DrumPattern> defaultDrumPatterns();

struct DrumEvent
{
    int offset = 0;          // sample offset inside the rendered block
    juce::uint8 note = 0;
    juce::uint8 velocity = 0; // 0 = note off
};
/** Event note that releases every sounding note (transport stopped or jumped). */
constexpr juce::uint8 allNotesOffNote = 255;

/** Collects the events of a pattern clip for [position, position + length).
    patternOffset is the pattern time (samples) at clipStart. Returns the
    number of events written (unsorted). */
int collectPatternEvents(const DrumPattern&, double samplesPerStep,
                         juce::int64 clipStart, juce::int64 clipEnd, double patternOffset,
                         juce::int64 position, int length, DrumEvent* out, int maxEvents) noexcept;
void sortEvents(DrumEvent* events, int count) noexcept;

/** The sound source of a drum track. */
class TrackInstrument
{
public:
    virtual ~TrackInstrument() = default;
    /** Message thread, before the instrument is published to the audio thread. */
    virtual void prepare(double sampleRate, int maximumBlockSize) = 0;
    /** Audio thread: overwrites left/right with numSamples and plays the
        sorted events plus any queued previews. */
    virtual void render(const DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept = 0;
    /** Message thread: an independent copy for offline export, or null. */
    [[nodiscard]] virtual std::shared_ptr<TrackInstrument> createOfflineCopy() const = 0;
    [[nodiscard]] virtual juce::String name() const = 0;
    /** Message thread: settings including the current plugin state, or an
        invalid tree if nothing changes at runtime (built-in kit). */
    [[nodiscard]] virtual juce::ValueTree currentSettings() { return {}; }

    /** Any non-audio thread: audition one hit or note (lock-free, single
        producer); velocity 0 releases a previewed note. */
    void preview(int note, int velocity) noexcept;

protected:
    /** Audio thread: appends queued previews (at offset 0) to out. */
    int takePreviews(DrumEvent* out, int maxEvents) noexcept;

private:
    static constexpr int queueSize = 64;
    std::array<std::atomic<int>, queueSize> queue {};
    std::atomic<int> writeIndex { 0 }, readIndex { 0 };
};

/** Built-in synthesized kit (808/909 flavoured), no samples or plugins needed. */
class DrumSynth final : public TrackInstrument
{
public:
    DrumSynth();
    void prepare(double sampleRate, int maximumBlockSize) override;
    void render(const DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept override;
    [[nodiscard]] std::shared_ptr<TrackInstrument> createOfflineCopy() const override { return std::make_shared<DrumSynth>(); }
    [[nodiscard]] juce::String name() const override { return "Internes Synth-Kit"; }

private:
    struct Voice
    {
        bool active = false;
        int lane = 0;
        float gain = 0.0f;
        double time = 0.0;      // seconds since trigger
        double phase = 0.0, phase2 = 0.0;
        std::array<double, 6> metal {};
        float lowPass = 0.0f, lowPass2 = 0.0f, highPass = 0.0f, previous = 0.0f;
        float fade = 1.0f;       // choke fade-out
        bool choking = false;
        juce::uint32 noise = 22222;
    };

    void trigger(int note, int velocity) noexcept;
    float renderVoice(Voice&) noexcept;

    double sampleRate = 48000.0;
    std::array<Voice, 32> voices;
    int nextVoice = 0;
};

/** Default instrument settings (the built-in kit). */
[[nodiscard]] juce::ValueTree builtInInstrumentSettings();
} // namespace daw
