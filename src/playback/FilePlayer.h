#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

// A mono/stereo WAV source decoded to mono before it ever becomes visible to the
// audio callback. Loaded sources deliberately remain alive until destruction:
// this makes exchanging a source lock-free and prevents deleting memory while
// the callback is reading it.
class FilePlayer final
{
public:
    enum class State : unsigned char { stopped, playing, paused };

    struct Status
    {
        State state = State::stopped;
        bool looping = false;
        bool hasFile = false;
        juce::int64 positionSamples = 0;
        juce::int64 lengthSamples = 0;
        double sampleRate = 0.0;
        juce::String fileName;
    };

    FilePlayer();
    ~FilePlayer() = default;
    FilePlayer(const FilePlayer&) = delete;
    FilePlayer& operator=(const FilePlayer&) = delete;

    // Call on a worker or message thread. Stereo is folded to (L + R) / 2;
    // mono is unchanged. No automatic normalisation or gain boost is applied.
    juce::Result load(const juce::File& file);
    // Call on a worker or message thread, never from the audio callback. The
    // current source is rendered to this rate before it is published, so
    // process() remains allocation- and lock-free. A rate <= 0 selects the
    // source's native sample rate.
    juce::Result setOutputSampleRate(double sampleRate);
    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;
    void setLooping(bool shouldLoop) noexcept;
    bool isLooping() const noexcept;
    void setPositionNormalized(float normalized) noexcept;
    float positionNormalized() const noexcept;

    // Audio-thread API. Replaces destination with the next mono source block,
    // zero-filling it when stopped or at end-of-file. The destination is owned
    // and preallocated by the caller.
    void process(float* destination, int numSamples) noexcept;
    Status getStatus() const;

private:
    struct DecodedSource
    {
        juce::AudioBuffer<float> samples;
        double sampleRate = 0.0;
        juce::String fileName;
    };

    struct Source
    {
        juce::AudioBuffer<float> samples;
        double sampleRate = 0.0;
        juce::String fileName;
    };

    static std::unique_ptr<Source> renderForOutputRate(const DecodedSource& decoded,
                                                        double outputSampleRate);
    juce::Result publishRenderedSource(const DecodedSource& decoded);

    juce::AudioFormatManager formats;
    std::vector<std::unique_ptr<DecodedSource>> retainedDecodedSources;
    std::vector<std::unique_ptr<Source>> retainedSources;
    std::atomic<Source*> current { nullptr };
    std::atomic<juce::int64> position { 0 };
    std::atomic<State> state { State::stopped };
    std::atomic<bool> looping { false };
    double outputSampleRate = 0.0;
    DecodedSource* latestDecodedSource = nullptr;
};
