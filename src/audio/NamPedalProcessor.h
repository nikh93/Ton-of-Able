#pragma once

#include <atomic>
#include <memory>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// Independent mono NAM pedal. Never shares state with the NAM amp.
// Control/prepare calls are non-audio; process is audio-thread only. Stop audio
// callbacks before destruction. The worker owns all DSP creation/destruction.
class NamPedalProcessor final : private juce::Thread
{
public:
    NamPedalProcessor();
    ~NamPedalProcessor() override;
    void prepare(double sampleRate, int maximumBlockSize);
    void set(const juce::String& filePath, bool bypass, float inputDb, float outputDb);
    // Parameter-only path: never requests a model load.
    void setControls(bool bypass, float inputDb, float outputDb) noexcept;
    void process(float* mono, int numSamples) noexcept;
    juce::String name() const;
    juce::String lastError() const;

private:
    struct Model;
    void run() override;
    mutable juce::CriticalSection mutex;
    juce::WaitableEvent wake;
    std::unique_ptr<Model> active;
    juce::String requestedPath, error;
    double rate = 48000.0;
    int maximumBlock = 512;
    std::atomic<uint64_t> generation { 1 }, faultGeneration { 0 };
    std::atomic<bool> bypassed { true };
    std::atomic<float> inputTrim { 0.0f }, outputTrim { 0.0f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NamPedalProcessor)
};
