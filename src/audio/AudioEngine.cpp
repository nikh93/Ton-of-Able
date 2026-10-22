#include "AudioEngine.h"

#include "presets/PresetManager.h"

#include <algorithm>

AudioEngine::AudioEngine()
{
    const auto dataDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Ton of Able");
    dataDirectory.createDirectory();
    logger.start(dataDirectory.getChildFile("Ton of Able.log"));
    // The initial device selection happens outside the audio callback. One mono
    // input feeds two physical outputs; processors will operate on mono only.
    // A second input is opened by default for dry arrangement tracks.
    const auto savedDeviceState = settings.loadAudioDeviceState();
    const auto error = manager.initialise(2, 2, savedDeviceState.get(), true);
    if (error.isNotEmpty())
    {
        const juce::ScopedLock lock(errorLock);
        lastDeviceError = error;
    }

    const auto switchedType = preferLowLatencyDeviceType(savedDeviceState == nullptr);
    if (savedDeviceState == nullptr || switchedType)
    {
        if (auto* device = manager.getCurrentAudioDevice())
        {
            auto setup = manager.getAudioDeviceSetup();
            const auto rates = device->getAvailableSampleRates();
            if (rates.contains(48000.0))
                setup.sampleRate = 48000.0;

            // Smallest buffer of at least 64 samples; below that NAM risks dropouts.
            const auto bufferSizes = device->getAvailableBufferSizes();
            int chosen = 0;
            for (const auto size : bufferSizes)
                if (size >= 64 && (chosen == 0 || size < chosen))
                    chosen = size;
            if (chosen == 0 && !bufferSizes.isEmpty())
                chosen = bufferSizes[0];
            if (chosen > 0)
                setup.bufferSize = chosen;

            manager.setAudioDeviceSetup(setup, true);
        }
    }
    manager.addAudioCallback(this);
}

AudioEngine::~AudioEngine()
{
    manager.removeAudioCallback(this);
    if (const auto state = manager.createStateXml())
        settings.saveAudioDeviceState(*state);
    manager.closeAudioDevice();
    recorder.stop();
    recorder.finaliseStoppedRecordings();
    logger.stop();
    settings.flush();
}

juce::Result AudioEngine::startRecording(const juce::File& target)
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr)
        return juce::Result::fail("Kein Audioausgabegerät geöffnet.");
    return recorder.start(target, device->getCurrentSampleRate(), 2);
}

AudioEngine::MeterSnapshot AudioEngine::meterSnapshot() const noexcept
{
    return {
        inputPeak.load(std::memory_order_relaxed),
        outputPeak.load(std::memory_order_relaxed),
        manager.getCpuUsage()
    };
}

double AudioEngine::estimatedLatencyMilliseconds() const
{
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr || device->getCurrentSampleRate() <= 0.0)
        return -1.0;
    const auto samples = device->getInputLatencyInSamples()
        + device->getCurrentBufferSizeSamples()
        + device->getOutputLatencyInSamples()
        + rig.latencySamples();
    return static_cast<double>(samples) * 1000.0 / device->getCurrentSampleRate();
}

juce::String AudioEngine::lastError() const
{
    const juce::ScopedLock lock(errorLock);
    return lastDeviceError;
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const auto rate = device->getCurrentSampleRate();
    const auto block = device->getCurrentBufferSizeSamples();
    activeSampleRate.store(rate, std::memory_order_relaxed);
    filePlayer.setOutputSampleRate(rate);
    monoBuffer.setSize(1, block, false, false, true);
    stereoBuffer.setSize(2, block, false, false, true);
    rig.prepare(rate, block);
    arrangementEngine.prepare(rate, block);
    inputPeak.store(0.0f, std::memory_order_relaxed);
    outputPeak.store(0.0f, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceStopped()
{
    inputPeak.store(0.0f, std::memory_order_relaxed);
    outputPeak.store(0.0f, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                    const int numInputChannels,
                                                    float* const* outputChannelData,
                                                    const int numOutputChannels,
                                                    const int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    // Host-owned processing avoids allocation, blocking waits, UI and file I/O.
    // VST3 slots use try-locks and dry fallback; third-party plugin internals
    // remain outside that guarantee. Models and IRs are prepared off-thread.
    performanceMonitor.beginBlock();
    float inPeak = 0.0f;
    float outPeak = 0.0f;
    const auto* input = numInputChannels > 0 ? inputChannelData[0] : nullptr;

    if (numSamples > monoBuffer.getNumSamples())
    {
        // A device must not exceed the buffer size announced in
        // audioDeviceAboutToStart. Fail safely rather than allocate in the
        // callback if a driver violates that contract.
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (auto* output = outputChannelData[channel])
                juce::FloatVectorOperations::clear(output, numSamples);
        performanceMonitor.reportXrun();
        performanceMonitor.endBlock(numSamples, activeSampleRate.load(std::memory_order_relaxed));
        return;
    }

    auto* mono = monoBuffer.getWritePointer(0);
    if (sourceMode.load(std::memory_order_relaxed) == SourceMode::wav)
        filePlayer.process(mono, numSamples);
    else if (input != nullptr)
        juce::FloatVectorOperations::copy(mono, input, numSamples);
    else
        juce::FloatVectorOperations::clear(mono, numSamples);

    auto* left = stereoBuffer.getWritePointer(0);
    auto* right = stereoBuffer.getWritePointer(1);
    inPeak = rig.processWithInputPeak(mono, left, right, numSamples);
    float* stereoChannels[] { left, right };
    juce::AudioBuffer<float> stereoView(stereoChannels, 2, numSamples);
    recorder.process(stereoView);
    // Multitrack playback, input monitoring and click are added on top of the
    // live rig. Tracks recording the rig read left/right before the mix is
    // added at the end of the arrangement block.
    arrangementEngine.process(inputChannelData, numInputChannels, left, right, left, right, numSamples);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (auto* output = outputChannelData[channel])
                output[sample] = numOutputChannels == 1 ? .5f * (left[sample] + right[sample])
                                                       : (channel % 2 == 0 ? left[sample] : right[sample]);

        outPeak = std::max(outPeak, std::max(std::abs(left[sample]), std::abs(right[sample])));
    }

    inputPeak.store(inPeak, std::memory_order_relaxed);
    outputPeak.store(outPeak, std::memory_order_relaxed);
    performanceMonitor.endBlock(numSamples, activeSampleRate.load(std::memory_order_relaxed));
}

bool AudioEngine::preferLowLatencyDeviceType(bool firstStart)
{
    // ASIO always wins. Without ASIO, exclusive WASAPI allows the shortest
    // buffers (e.g. 144 instead of 480 samples on onboard Realtek), but it
    // cannot open while another application holds the device. Then fall back
    // to the low-latency shared mode and finally plain shared WASAPI. A saved
    // choice that still opens is kept. Returns true if the type was changed.
    const auto deviceOpen = [this]
    {
        auto* device = manager.getCurrentAudioDevice();
        return device != nullptr && device->isOpen();
    };
    const auto hasDevices = [this](const juce::String& name)
    {
        for (const auto* type : manager.getAvailableDeviceTypes())
            if (type->getTypeName() == name)
                return !type->getDeviceNames().isEmpty();
        return false;
    };

    bool switched = false;
    for (const juce::String name : { "ASIO", "Windows Audio (Exclusive Mode)",
                                     "Windows Audio (Low Latency Mode)", "Windows Audio" })
    {
        if (name != "ASIO" && !firstStart && deviceOpen())
            return switched;
        if (!hasDevices(name))
            continue;
        if (manager.getCurrentAudioDeviceType() == name && deviceOpen())
            return switched;
        manager.setCurrentAudioDeviceType(name, true);
        switched = true;
        if (deviceOpen())
            return true;
    }
    return switched;
}
