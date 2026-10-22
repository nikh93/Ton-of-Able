#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <tuple>

#include <juce_events/juce_events.h>

#include "AudioData.h"

namespace daw
{
/** Offline time-stretch and pitch-shift with Rubber Band Library (R3 "finer"
    engine, offline mode). The result has exactly round(length * timeRatio)
    samples and is time-aligned with the input. Returns null if cancelled. */
[[nodiscard]] std::shared_ptr<AudioData> renderStretched(const AudioData& source,
                                                         double timeRatio,
                                                         double pitchSemitones,
                                                         const std::atomic<bool>* cancelled = nullptr);

/** Runs renderStretched() on background threads and reports on the message
    thread. Identical requests (same source, ratio and pitch) share one job. */
class StretchService final
{
public:
    using Callback = std::function<void(AudioDataPtr source, double ratio, double pitch, AudioDataPtr rendered)>;

    explicit StretchService(Callback onRendered);
    ~StretchService();

    void request(const AudioDataPtr& source, double ratio, double pitch);
    /** Cancels jobs whose result is no longer referenced by isWanted. */
    void cancelUnwanted(const std::function<bool(const AudioData*, double, double)>& isWanted);
    [[nodiscard]] int pendingJobs() const noexcept { return static_cast<int>(jobs.size()); }

private:
    struct Key
    {
        const AudioData* source;
        juce::int64 ratio;   // fixed point to make keys exact
        juce::int64 pitch;
        bool operator<(const Key& other) const noexcept
        {
            return std::tie(source, ratio, pitch) < std::tie(other.source, other.ratio, other.pitch);
        }
    };
    struct Job;
    static Key keyFor(const AudioData*, double ratio, double pitch) noexcept;

    Callback onRendered;
    juce::ThreadPool pool { juce::ThreadPoolOptions {}.withThreadName("Rubber Band").withNumberOfThreads(2) };
    std::map<Key, std::shared_ptr<Job>> jobs;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};
} // namespace daw
