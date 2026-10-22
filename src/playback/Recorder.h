#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

// Disk I/O is performed by JUCE's TimeSliceThread. The callback only pushes
// already-rendered samples into ThreadedWriter's bounded FIFO.
class Recorder final
{
public:
    Recorder();
    ~Recorder();
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    juce::Result start(const juce::File& target, double sampleRate, unsigned int numChannels = 1);
    void stop() noexcept;
    bool isRecording() const noexcept;

    // Audio-thread API. channels must remain valid for this call only.
    void process(const float* const* channels, int numChannels, int numSamples) noexcept;
    void process(const juce::AudioBuffer<float>& buffer) noexcept;

    // Call from a non-realtime thread after stop(). It waits for active
    // callbacks to leave before closing completed WAV files.
    void finaliseStoppedRecordings();

private:
    struct Session
    {
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    };

    juce::TimeSliceThread diskThread { "NAM WAV writer" };
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<unsigned int> activeChannelCount { 0 };
    std::atomic<unsigned int> callbacksInFlight { 0 };
    std::vector<std::unique_ptr<Session>> sessions;
    juce::CriticalSection sessionLock;
};
