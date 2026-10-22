#include "NamPedalProcessor.h"

#include "NAM/get_dsp.h"
#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<NAM_SAMPLE, float>, "Compile NAM pedal/core with NAM_SAMPLE_FLOAT");

struct NamPedalProcessor::Model
{
    std::unique_ptr<nam::DSP> dsp;
    juce::AudioBuffer<float> work;
    juce::SmoothedValue<float> input, output, wet;
    juce::String name;
    uint64_t generation = 0;
};

NamPedalProcessor::NamPedalProcessor() : Thread("NAM pedal loader")
{
    startThread(juce::Thread::Priority::normal);
}

NamPedalProcessor::~NamPedalProcessor()
{
    signalThreadShouldExit();
    wake.signal();
    // Never forcibly terminate a thread inside third-party model construction.
    waitForThreadToExit(-1);
}

void NamPedalProcessor::setControls(bool bypass, float inputDb, float outputDb) noexcept
{
    inputTrim.store(std::isfinite(inputDb) ? juce::jlimit(-24.0f, 24.0f, inputDb) : 0.0f);
    outputTrim.store(std::isfinite(outputDb) ? juce::jlimit(-24.0f, 24.0f, outputDb) : 0.0f);
    bypassed.store(bypass);
}

void NamPedalProcessor::set(const juce::String& filePath, bool bypass, float inputDb, float outputDb)
{
    // File construction/validation occurs on the worker; no disk I/O on knobs.
    const auto path = filePath.trim();
    const juce::ScopedLock lock(mutex);
    if (path != requestedPath)
    {
        requestedPath = path;
        error = path.isEmpty() ? juce::String{} : juce::String("Loading NAM pedal...");
        generation.fetch_add(1);
        wake.signal();
    }
    setControls(bypass, inputDb, outputDb);
}

void NamPedalProcessor::prepare(double sampleRate, int maximumBlockSize)
{
    const juce::ScopedLock lock(mutex);
    // Rebuild even for the same format on device restart, to clear model history.
    rate = sampleRate;
    maximumBlock = maximumBlockSize;
    error = requestedPath.isEmpty() ? juce::String{} : juce::String("Loading NAM pedal...");
    generation.fetch_add(1);
    wake.signal();
}

juce::String NamPedalProcessor::name() const
{
    const juce::ScopedLock lock(mutex);
    const auto current = generation.load();
    return active && active->generation == current && faultGeneration.load() != current
        ? active->name : juce::String{};
}

juce::String NamPedalProcessor::lastError() const
{
    const juce::ScopedLock lock(mutex);
    if (faultGeneration.load() == generation.load())
        return "NAM pedal DSP failed or produced non-finite audio; bypassed until path change or prepare.";
    return error;
}

void NamPedalProcessor::run()
{
    uint64_t handled = 0;
    while (!threadShouldExit())
    {
        wake.wait(100);
        juce::String path;
        double sampleRate;
        int block;
        uint64_t request;
        {
            const juce::ScopedLock lock(mutex);
            request = generation.load();
            if (request == handled) continue;
            handled = request;
            path = requestedPath;
            sampleRate = rate;
            block = maximumBlock;
        }
        std::unique_ptr<Model> candidate;
        juce::String failure;
        try
        {
            if (path.isNotEmpty())
            {
                if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 768000.0
                    || block < 1 || block > 65536)
                    throw std::runtime_error("Unsupported pedal processing format");
                if (!juce::File::isAbsolutePath(path))
                    throw std::runtime_error("NAM pedal path must be absolute");
                const juce::File file(path);
                if (!file.existsAsFile()) throw std::runtime_error("NAM pedal file is missing");
                nam::DspLoadOptions options;
                options.prewarm = false;
                candidate = std::make_unique<Model>();
                candidate->dsp = nam::get_dsp(std::filesystem::path(path.toWideCharPointer()), options);
                auto& dsp = *candidate->dsp;
                if (dsp.NumInputChannels() != 1 || dsp.NumOutputChannels() != 1)
                    throw std::runtime_error("NAM pedal requires a mono input/output model");
                const double expected = dsp.GetExpectedSampleRate();
                // Reset is not resampling. Unknown legacy rates use device rate;
                // known mismatches are rejected rather than changing pedal tone.
                if (expected > 0.0 && std::abs(expected - sampleRate) > 0.5)
                    throw std::runtime_error("NAM pedal model sample rate does not match the device");
                dsp.SetPrewarmOnReset(false);
                dsp.Reset(sampleRate, block);
                dsp.prewarm();
                candidate->work.setSize(1, block);
                candidate->generation = request;
                candidate->name = file.getFileNameWithoutExtension();
                candidate->input.reset(sampleRate, 0.020);
                candidate->output.reset(sampleRate, 0.020);
                candidate->wet.reset(sampleRate, 0.010);
                candidate->input.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputTrim.load()));
                candidate->output.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputTrim.load()));
                candidate->wet.setCurrentAndTargetValue(0.0f);
            }
        }
        catch (const std::exception& e) { candidate.reset(); failure = e.what(); }
        catch (...) { candidate.reset(); failure = "Unknown NAM pedal loading error"; }
        {
            const juce::ScopedLock lock(mutex);
            if (request == generation.load() && !threadShouldExit())
            {
                // Swap under the lock, destroy the retired model below on worker.
                active.swap(candidate);
                error = failure;
            }
        }
    }
    const juce::ScopedLock lock(mutex);
    active.reset();
}

void NamPedalProcessor::process(float* mono, int count) noexcept
{
    if (mono == nullptr || count <= 0) return;
    const juce::ScopedTryLock lock(mutex);
    if (!lock.isLocked()) return;
    const auto current = generation.load();
    if (!active || active->generation != current || faultGeneration.load() == current
        || count > active->work.getNumSamples()) return;
    auto& model = *active;
    model.input.setTargetValue(juce::Decibels::decibelsToGain(inputTrim.load()));
    model.output.setTargetValue(juce::Decibels::decibelsToGain(outputTrim.load()));
    model.wet.setTargetValue(bypassed.load() ? 0.0f : 1.0f);
    auto* work = model.work.getWritePointer(0);
    for (int i = 0; i < count; ++i)
    {
        work[i] = mono[i] * model.input.getNextValue();
        if (!std::isfinite(work[i])) { faultGeneration.store(current); return; }
    }
    try
    {
        NAM_SAMPLE* channels[] { work };
        model.dsp->process(channels, channels, count);
    }
    catch (...) { faultGeneration.store(current); return; }
    // Validate the entire block before touching the caller's dry buffer.
    for (int i = 0; i < count; ++i)
    {
        const double wet = model.wet.getNextValue();
        const double gain = model.output.getNextValue();
        const double y = (1.0 - wet) * mono[i] + wet * static_cast<double>(work[i]) * gain;
        if (!std::isfinite(work[i]) || !std::isfinite(y) || std::abs(y) > std::numeric_limits<float>::max())
        { faultGeneration.store(current); return; }
        work[i] = static_cast<float>(y);
    }
    juce::FloatVectorOperations::copy(mono, work, count);
}
