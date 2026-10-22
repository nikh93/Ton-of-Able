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
    }
    setError({});
    workAvailable.signal();
}

void NamProcessor::setProcessingFormat(const double sampleRate, const int maximumBlockSize) noexcept
{
    currentSampleRate.store(sampleRate > 0.0 ? sampleRate : 48000.0, std::memory_order_relaxed);
    currentBlockSize.store(juce::jmax(1, maximumBlockSize), std::memory_order_relaxed);
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

    NAM_SAMPLE* channels[] { monoSamples };
    activeModel->dsp->process(channels, channels, numSamples);
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
            requestedFile = {};
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
        dsp->Reset(currentSampleRate.load(std::memory_order_relaxed), currentBlockSize.load(std::memory_order_relaxed));
        dsp->prewarm();

        auto* model = new ModelInstance { std::move(dsp), modelFile.getFileNameWithoutExtension() };
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
