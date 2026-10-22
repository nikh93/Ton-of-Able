#include "TimeStretcher.h"

#include <cmath>

#include <rubberband/RubberBandStretcher.h>

namespace daw
{
std::shared_ptr<AudioData> renderStretched(const AudioData& source, double timeRatio, double pitchSemitones, const std::atomic<bool>* cancelled)
{
    using RubberBand::RubberBandStretcher;
    const auto channels = source.numChannels();
    const auto inputLength = static_cast<size_t>(source.length());
    if (channels <= 0 || inputLength == 0 || source.sampleRate <= 0.0)
        return {};

    const auto isCancelled = [cancelled] { return cancelled != nullptr && cancelled->load(std::memory_order_relaxed); };
    const RubberBandStretcher::Options options = RubberBandStretcher::OptionProcessOffline
        | RubberBandStretcher::OptionEngineFiner
        | RubberBandStretcher::OptionChannelsTogether
        | RubberBandStretcher::OptionThreadingNever;
    RubberBandStretcher stretcher(static_cast<size_t>(std::lround(source.sampleRate)),
                                  static_cast<size_t>(channels), options,
                                  timeRatio, std::pow(2.0, pitchSemitones / 12.0));
    stretcher.setExpectedInputDuration(inputLength);

    constexpr size_t blockSize = 4096;
    stretcher.setMaxProcessSize(blockSize);
    std::vector<const float*> input(static_cast<size_t>(channels));
    const auto pointAt = [&](size_t position)
    {
        for (int channel = 0; channel < channels; ++channel)
            input[static_cast<size_t>(channel)] = source.samples.getReadPointer(channel) + position;
    };

    // Offline mode: one study pass over the whole clip, then processing.
    for (size_t position = 0; position < inputLength; position += blockSize)
    {
        if (isCancelled())
            return {};
        const auto count = std::min(blockSize, inputLength - position);
        pointAt(position);
        stretcher.study(input.data(), count, position + count >= inputLength);
    }

    const auto expectedLength = static_cast<int>(std::llround(static_cast<double>(inputLength) * timeRatio));
    auto result = std::make_shared<AudioData>();
    result->sampleRate = source.sampleRate;
    result->samples.setSize(channels, expectedLength + static_cast<int>(blockSize), false, true, false);
    int written = 0;
    std::vector<float*> output(static_cast<size_t>(channels));
    const auto drain = [&]
    {
        for (auto available = stretcher.available(); available > 0; available = stretcher.available())
        {
            if (written + available > result->samples.getNumSamples())
                result->samples.setSize(channels, written + available + static_cast<int>(blockSize), true, true, false);
            for (int channel = 0; channel < channels; ++channel)
                output[static_cast<size_t>(channel)] = result->samples.getWritePointer(channel) + written;
            written += static_cast<int>(stretcher.retrieve(output.data(), static_cast<size_t>(available)));
        }
    };

    for (size_t position = 0; position < inputLength; position += blockSize)
    {
        if (isCancelled())
            return {};
        const auto count = std::min(blockSize, inputLength - position);
        pointAt(position);
        stretcher.process(input.data(), count, position + count >= inputLength);
        drain();
    }
    // Without internal threads, all remaining output is available right after
    // the final block; available() then reports -1.
    drain();

    // Pin the length so that timeline maths stays exact.
    result->samples.setSize(channels, expectedLength, true, true, false);
    if (written < expectedLength)
        result->samples.clear(written, expectedLength - written);
    result->computePeaks();
    return result;
}

struct StretchService::Job
{
    AudioDataPtr source;
    double ratio = 1.0;
    double pitch = 0.0;
    std::atomic<bool> cancelled { false };
};

StretchService::StretchService(Callback callback) : onRendered(std::move(callback)) {}

StretchService::~StretchService()
{
    alive.reset();
    for (auto& entry : jobs)
        entry.second->cancelled = true;
    pool.removeAllJobs(true, 30000);
}

StretchService::Key StretchService::keyFor(const AudioData* source, double ratio, double pitch) noexcept
{
    return { source, std::llround(ratio * 1.0e6), std::llround(pitch * 1.0e6) };
}

void StretchService::request(const AudioDataPtr& source, double ratio, double pitch)
{
    if (source == nullptr)
        return;
    const auto key = keyFor(source.get(), ratio, pitch);
    if (const auto existing = jobs.find(key); existing != jobs.end() && ! existing->second->cancelled)
        return;

    auto job = std::make_shared<Job>();
    job->source = source;
    job->ratio = ratio;
    job->pitch = pitch;
    jobs[key] = job;
    std::weak_ptr<bool> weakAlive = alive;
    pool.addJob([this, job, key, weakAlive]
    {
        std::shared_ptr<const AudioData> rendered = renderStretched(*job->source, job->ratio, job->pitch, &job->cancelled);
        juce::MessageManager::callAsync([this, job, key, weakAlive, rendered]
        {
            if (weakAlive.expired())
                return;
            if (const auto found = jobs.find(key); found != jobs.end() && found->second == job)
                jobs.erase(found);
            if (! job->cancelled && rendered != nullptr && onRendered)
                onRendered(job->source, job->ratio, job->pitch, rendered);
        });
    });
}

void StretchService::cancelUnwanted(const std::function<bool(const AudioData*, double, double)>& isWanted)
{
    for (auto it = jobs.begin(); it != jobs.end();)
    {
        if (! isWanted(it->second->source.get(), it->second->ratio, it->second->pitch))
        {
            it->second->cancelled = true;
            it = jobs.erase(it);
        }
        else
            ++it;
    }
}
} // namespace daw
