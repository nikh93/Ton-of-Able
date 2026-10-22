#pragma once

#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

#include "FlipFlopOctaver.h"
#include "RTUtilities.h"

namespace RubberBand
{
class RubberBandStretcher;
class RubberBandLiveShifter;
}

namespace nam::effects
{
// Live pitch shifting (e.g. an octave down) with Rubber Band, usable while
// playing through the rig.
//
// Two engines are prepared up front and switched without allocation:
//  - lowLatency: RubberBandStretcher in real-time mode, R2 engine with the
//    short window; measured about 20-36 ms an octave down at 48 kHz,
//    depending on the device block size - the playable default;
//  - highQuality: RubberBandLiveShifter (R3), cleaner but about 87 ms late.
// Exactly one octave down (-12) always uses the analog-style flip-flop
// octaver instead: no latency, monophonic, like an octave pedal.
// The dry signal is delayed by the same amount, so blending never flams.
class LivePitchShifter final
{
public:
    enum class Mode : int { lowLatency = 0, highQuality = 1 };

    LivePitchShifter();
    ~LivePitchShifter();

    /** Message/lifecycle thread: allocates both engines. */
    void prepare(double sampleRate, int maximumBlockSize);

    void setBypassed(bool value) noexcept { bypassed.store(value); }
    void setSemitones(float value) noexcept { semitones.store(juce::jlimit(-24.0f, 24.0f, value)); }
    void setMix(float value) noexcept { mix.store(juce::jlimit(0.0f, 1.0f, value)); }
    void setFormantPreserved(bool value) noexcept { formant.store(value); }
    void setMode(Mode value) noexcept { mode.store(static_cast<int>(value)); }

    /** True when -12 is set and the latency-free flip-flop octaver runs instead of Rubber Band. */
    [[nodiscard]] static bool usesAnalogOctave(float semitoneValue) noexcept { return std::abs(semitoneValue + 12.0f) < 1.0e-3f; }

    /** Processing delay in samples while active (0 when bypassed or for -12). */
    [[nodiscard]] int latencySamples() const noexcept;

    /** Audio thread, in place, mono. */
    void process(float* mono, int numSamples) noexcept;
    /** Samples the engine could not deliver in time (diagnostics and tests). */
    [[nodiscard]] int underrunCount() const noexcept { return underruns; }

private:
    struct Ring
    {
        std::vector<float> data;
        size_t read = 0, write = 0, size = 0;
        void reset(size_t capacity) { data.assign(capacity, 0.0f); read = write = size = 0; }
        void push(float value) noexcept { data[write] = value; write = (write + 1) % data.size(); ++size; }
        float pop() noexcept { const auto value = data[read]; read = (read + 1) % data.size(); --size; return value; }
    };

    void restartEngine(int activeMode) noexcept;
    void runLowLatency(const float* input, int numSamples) noexcept;
    void runHighQuality(const float* input, int numSamples) noexcept;

    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    std::unique_ptr<RubberBand::RubberBandLiveShifter> liveShifter;
    FlipFlopOctaver octaver;
    static constexpr int analogEngine = 2;
    std::atomic<bool> bypassed { true }, formant { false };
    std::atomic<float> semitones { -12.0f }, mix { 1.0f };
    std::atomic<int> mode { 0 };

    // Audio-thread state.
    double rate = 48000.0;
    int maximumBlock = 512;
    int runningMode = -1;
    double appliedScale = 0.0;
    float appliedSemitones = 0.0f;
    bool appliedFormant = false;
    int delaySamples = 0;          // fixed delay of the active engine incl. buffering
    int pendingDiscard = 0;
    bool primed = false;
    size_t inputCounter = 0;
    int underruns = 0;
    bool active = false;
    Ring wet, liveInput;
    std::vector<float> dryDelay;   // circular line aligning the dry signal
    size_t dryPosition = 0;
    std::vector<float> scratchIn, scratchOut;
    BypassRamp bypassRamp;
    juce::SmoothedValue<float> mixSmoothed;
};
} // namespace nam::effects
