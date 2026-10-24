#pragma once

#include <array>
#include <atomic>

#include <juce_core/juce_core.h>

namespace nam
{
class DSP;
}

class NamProcessor final : private juce::Thread
{
public:
    NamProcessor();
    ~NamProcessor() override;

    NamProcessor(const NamProcessor&) = delete;
    NamProcessor& operator=(const NamProcessor&) = delete;

    // May be called from the message thread. File parsing, allocation, Reset and
    // prewarming all execute on this object's worker thread.
    void loadAsync(const juce::File& modelFile);
    // Non-audio thread. A changed format re-prepares the loaded model on the
    // worker; until it arrives, process() feeds the old one in blocks it fits.
    void setProcessingFormat(double sampleRate, int maximumBlockSize);

    // Audio-thread only. `monoSamples` is processed in-place by a fully prepared
    // NAM model, or left untouched while no model is active.
    void process(float* monoSamples, int numSamples) noexcept;
    bool hasActiveModel() const noexcept;
    juce::String activeModelName() const;
    juce::String lastError() const;

private:
    struct ModelInstance
    {
        std::unique_ptr<nam::DSP> dsp;
        juce::String displayName;
        int maximumBlock = 1; // the block size the model was Reset with
    };

    template <size_t capacity>
    class SpscModelQueue
    {
    public:
        bool tryPush(ModelInstance* value) noexcept
        {
            const auto write = writeIndex.load(std::memory_order_relaxed);
            const auto next = (write + 1) % capacity;
            if (next == readIndex.load(std::memory_order_acquire))
                return false;
            entries[write] = value;
            writeIndex.store(next, std::memory_order_release);
            return true;
        }

        ModelInstance* tryPop() noexcept
        {
            const auto read = readIndex.load(std::memory_order_relaxed);
            if (read == writeIndex.load(std::memory_order_acquire))
                return nullptr;
            auto* value = entries[read];
            readIndex.store((read + 1) % capacity, std::memory_order_release);
            return value;
        }

        bool canPush() const noexcept
        {
            const auto write = writeIndex.load(std::memory_order_relaxed);
            return (write + 1) % capacity != readIndex.load(std::memory_order_acquire);
        }

    private:
        std::array<ModelInstance*, capacity> entries {};
        std::atomic<size_t> readIndex { 0 };
        std::atomic<size_t> writeIndex { 0 };
    };

    void run() override;
    void loadPrepared(const juce::File& modelFile);
    void collectRetiredModels() noexcept;
    void setError(juce::String error);

    static constexpr size_t queueCapacity = 8;
    SpscModelQueue<queueCapacity> preparedModels;
    SpscModelQueue<queueCapacity> retiredModels;
    ModelInstance* activeModel = nullptr; // Accessed by the audio thread only.

    juce::WaitableEvent workAvailable;
    juce::CriticalSection requestLock;
    juce::File requestedFile;
    juce::File loadedFile; // last requested model, reloaded on format changes
    juce::CriticalSection stateLock;
    juce::String activeName;
    juce::String failureReason;
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> currentBlockSize { 512 };
};
