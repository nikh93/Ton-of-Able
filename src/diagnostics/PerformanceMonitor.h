#pragma once

#include <atomic>
#include <chrono>

// Lock-free aggregate timing for the audio callback. It intentionally reports
// a measured processing ratio, not a guessed end-to-end latency.
class PerformanceMonitor final
{
public:
    struct Snapshot
    {
        double averageMilliseconds = 0.0;
        double peakMilliseconds = 0.0;
        double realtimeLoad = 0.0;
        unsigned long long processedBlocks = 0;
        unsigned long long xruns = 0;
    };

    void beginBlock() noexcept;
    void endBlock(int numSamples, double sampleRate) noexcept;
    void reportXrun() noexcept;
    void reset() noexcept;
    Snapshot snapshot() const noexcept;

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point blockStart {};
    std::atomic<unsigned long long> totalNanoseconds { 0 };
    std::atomic<unsigned long long> peakNanoseconds { 0 };
    std::atomic<unsigned long long> blocks { 0 };
    std::atomic<unsigned long long> xruns { 0 };
    std::atomic<double> latestRealtimeLoad { 0.0 };
};
