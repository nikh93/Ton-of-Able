#include "Recorder.h"

Recorder::Recorder()
{
    diskThread.startThread();
}

Recorder::~Recorder()
{
    stop();
    while (callbacksInFlight.load(std::memory_order_acquire) != 0)
        juce::Thread::yield();
    finaliseStoppedRecordings();
    diskThread.stopThread(2000);
}

juce::Result Recorder::start(const juce::File& target, const double sampleRate, const unsigned int numChannels)
{
    if (sampleRate <= 0.0 || numChannels == 0)
        return juce::Result::fail("Invalid recording format.");
    if (isRecording())
        return juce::Result::fail("Recording is already active.");
    if (! target.getParentDirectory().exists() && ! target.getParentDirectory().createDirectory())
        return juce::Result::fail("Recording directory cannot be created.");

    auto stream = std::unique_ptr<juce::FileOutputStream>(target.createOutputStream());
    if (stream == nullptr || ! stream->openedOk())
        return juce::Result::fail("Recording destination cannot be opened.");

    juce::WavAudioFormat wav;
    auto* rawWriter = wav.createWriterFor(stream.release(), sampleRate, numChannels, 24, {}, 0);
    if (rawWriter == nullptr)
        return juce::Result::fail("Could not create WAV writer.");

    auto session = std::make_unique<Session>();
    session->writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(rawWriter, diskThread, 32768);
    auto* published = session->writer.get();
    {
        const juce::ScopedLock lock(sessionLock);
        sessions.push_back(std::move(session));
    }
    activeChannelCount.store(numChannels, std::memory_order_release);
    activeWriter.store(published, std::memory_order_release);
    return juce::Result::ok();
}

void Recorder::stop() noexcept
{
    activeWriter.store(nullptr, std::memory_order_release);
    activeChannelCount.store(0, std::memory_order_release);
}
bool Recorder::isRecording() const noexcept { return activeWriter.load(std::memory_order_acquire) != nullptr; }

void Recorder::process(const float* const* channels, const int numChannels, const int numSamples) noexcept
{
    callbacksInFlight.fetch_add(1, std::memory_order_acq_rel);
    const auto expectedChannels = activeChannelCount.load(std::memory_order_acquire);
    if (auto* writer = activeWriter.load(std::memory_order_acquire); writer != nullptr && channels != nullptr && numChannels >= static_cast<int>(expectedChannels) && expectedChannels > 0 && numSamples > 0)
        writer->write(channels, numSamples);
    callbacksInFlight.fetch_sub(1, std::memory_order_release);
}

void Recorder::process(const juce::AudioBuffer<float>& buffer) noexcept
{
    process(buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

void Recorder::finaliseStoppedRecordings()
{
    if (isRecording() || callbacksInFlight.load(std::memory_order_acquire) != 0)
        return;
    const juce::ScopedLock lock(sessionLock);
    sessions.clear(); // ThreadedWriter destructor drains its FIFO and finalises WAV headers.
}
