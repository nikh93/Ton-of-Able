#include "PerformanceMonitor.h"

#include <algorithm>

void PerformanceMonitor::beginBlock() noexcept
{
    blockStart = Clock::now();
}

void PerformanceMonitor::endBlock(const int numSamples, const double sampleRate) noexcept
{
    const auto elapsed = static_cast<unsigned long long>(std::max<long long>(0,
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - blockStart).count()));
    totalNanoseconds.fetch_add(elapsed, std::memory_order_relaxed);
    blocks.fetch_add(1, std::memory_order_relaxed);

    auto previousPeak = peakNanoseconds.load(std::memory_order_relaxed);
    while (elapsed > previousPeak && ! peakNanoseconds.compare_exchange_weak(previousPeak, elapsed, std::memory_order_relaxed)) {}

    const auto budget = sampleRate > 0.0 && numSamples > 0
        ? static_cast<double>(numSamples) * 1.0e9 / sampleRate : 0.0;
    latestRealtimeLoad.store(budget > 0.0 ? static_cast<double>(elapsed) / budget : 0.0, std::memory_order_relaxed);
}

void PerformanceMonitor::reportXrun() noexcept { xruns.fetch_add(1, std::memory_order_relaxed); }

void PerformanceMonitor::reset() noexcept
{
    totalNanoseconds.store(0, std::memory_order_relaxed);
    peakNanoseconds.store(0, std::memory_order_relaxed);
    blocks.store(0, std::memory_order_relaxed);
    xruns.store(0, std::memory_order_relaxed);
    latestRealtimeLoad.store(0.0, std::memory_order_relaxed);
}

PerformanceMonitor::Snapshot PerformanceMonitor::snapshot() const noexcept
{
    const auto count = blocks.load(std::memory_order_relaxed);
    const auto total = totalNanoseconds.load(std::memory_order_relaxed);
    return { count == 0 ? 0.0 : static_cast<double>(total) / static_cast<double>(count) / 1.0e6,
             static_cast<double>(peakNanoseconds.load(std::memory_order_relaxed)) / 1.0e6,
             latestRealtimeLoad.load(std::memory_order_relaxed), count,
             xruns.load(std::memory_order_relaxed) };
}
