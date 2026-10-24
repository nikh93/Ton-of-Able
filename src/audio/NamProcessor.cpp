#include "NamProcessor.h"

#include <chrono>
#include <thread>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

NamProcessor::NamProcessor()
    : Thread("NAM model loader")
{
    startThread(juce::Thread::Priority::normal);
}

NamProcessor::~NamProcessor()
{
    signalThreadShouldExit();
    workAvailable.signal();
    stopThread(5000);

    delete activeModel;
    while (auto* model = preparedModels.tryPop())
        delete model;
    while (auto* model = retiredModels.tryPop())
        delete model;
}

void NamProcessor::loadAsync(const juce::File& modelFile)
{
    if (!modelFile.existsAsFile())
    {
        setError("NAM-Datei nicht gefunden: " + modelFile.getFullPathName());
        return;
    }

    {
        const juce::ScopedLock lock(requestLock);
        requestedFile = modelFile;
        loadedFile = modelFile;
    }
    setError({});
    workAvailable.signal();
}

void NamProcessor::setProcessingFormat(const double sampleRate, const int maximumBlockSize)
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto block = juce::jmax(1, maximumBlockSize);
    const auto previousRate = currentSampleRate.exchange(rate, std::memory_order_relaxed);
    const auto previousBlock = currentBlockSize.exchange(block, std::memory_order_relaxed);
    if (previousRate == rate && previousBlock == block)
        return;

    // The active model's buffers were sized by Reset() for the old format.
    const juce::ScopedLock lock(requestLock);
    if (loadedFile == juce::File())
        return;
    juce::Logger::writeToLog("[NAM] format " + juce::String(previousRate) + " Hz / " + juce::String(previousBlock) + " -> "
                             + juce::String(rate) + " Hz / " + juce::String(block) + "; re-preparing " + loadedFile.getFileName());
    requestedFile = loadedFile;
    workAvailable.signal();
}

void NamProcessor::process(float* monoSamples, const int numSamples) noexcept
{
    // Do not exchange a live model when the worker has not yet drained the
    // retired queue. This preserves ownership without a lock or delete in the
    // callback; the next audio block will retry the handoff.
    if ((!activeModel || retiredModels.canPush()))
    {
        if (auto* nextModel = preparedModels.tryPop())
        {
            auto* previous = activeModel;
            activeModel = nextModel;
            if (previous != nullptr)
                [[maybe_unused]] const auto retired = retiredModels.tryPush(previous);
        }
    }

    if (activeModel == nullptr || monoSamples == nullptr || numSamples <= 0)
        return;

    // Never hand NAM more frames than its buffers were Reset for; a device with
    // a larger block runs the model in pieces until the re-prepared one arrives.
    const auto chunk = activeModel->maximumBlock;
    for (int offset = 0; offset < numSamples; offset += chunk)
    {
        NAM_SAMPLE* channels[] { monoSamples + offset };
        activeModel->dsp->process(channels, channels, juce::jmin(chunk, numSamples - offset));
    }
}

bool NamProcessor::hasActiveModel() const noexcept
{
    return activeModel != nullptr;
}

juce::String NamProcessor::activeModelName() const
{
    const juce::ScopedLock lock(stateLock);
    return activeName;
}

juce::String NamProcessor::lastError() const
{
    const juce::ScopedLock lock(stateLock);
    return failureReason;
}

void NamProcessor::run()
{
    while (!threadShouldExit())
    {
        workAvailable.wait(250);
        collectRetiredModels();

        juce::File modelFile;
        {
            const juce::ScopedLock lock(requestLock);
            modelFile = requestedFile;
            requestedFile = juce::File();
        }

        if (modelFile.existsAsFile())
            loadPrepared(modelFile);
    }

    collectRetiredModels();
}

void NamProcessor::loadPrepared(const juce::File& modelFile)
{
    try
    {
        nam::DspLoadOptions options;
        options.prewarm = false;
        auto dsp = nam::get_dsp(std::filesystem::path(modelFile.getFullPathName().toStdString()), options);
        dsp->SetPrewarmOnReset(false);
        const auto rate = currentSampleRate.load(std::memory_order_relaxed);
        const auto block = currentBlockSize.load(std::memory_order_relaxed);
        dsp->Reset(rate, block);
        dsp->prewarm();

        auto* model = new ModelInstance { std::move(dsp), modelFile.getFileNameWithoutExtension(), block };
        juce::Logger::writeToLog("[NAM] prepared " + modelFile.getFileName() + " for " + juce::String(rate) + " Hz, block "
                                 + juce::String(block));
        while (!threadShouldExit() && !preparedModels.tryPush(model))
        {
            collectRetiredModels();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (threadShouldExit())
        {
            delete model;
            return;
        }

        {
            const juce::ScopedLock lock(stateLock);
            activeName = model->displayName;
            failureReason.clear();
        }
    }
    catch (const std::exception& exception)
    {
        setError("NAM konnte nicht geladen werden: " + juce::String(exception.what()));
    }
}

void NamProcessor::collectRetiredModels() noexcept
{
    while (auto* model = retiredModels.tryPop())
        delete model;
}

void NamProcessor::setError(juce::String error)
{
    const juce::ScopedLock lock(stateLock);
    failureReason = std::move(error);
}
