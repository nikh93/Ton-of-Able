#include "AudioEngine.h"

#include "diagnostics/CrashHandler.h"
#include "presets/PresetManager.h"

#include <algorithm>
#include <cstdio>

AudioEngine::AudioEngine()
{
    keyboardMidi.ensureSize(4096);
    const auto dataDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Ton of Able");
    dataDirectory.createDirectory();
    logger.start(dataDirectory.getChildFile("Ton of Able.log"));
    crashlog::install(logger.file());
    juce::Logger::setCurrentLogger(&juceLogForwarder);
    logger.log(Logger::Level::info, "Ton of Able started on " + juce::SystemStats::getOperatingSystemName()
               + (juce::SystemStats::isOperatingSystem64Bit() ? " (64 bit)" : " (32 bit)") + ", CPU " + juce::SystemStats::getCpuModel()
               + ", build " + juce::String(__DATE__) + " " + juce::String(__TIME__) + ", log " + logger.file().getFullPathName());
    // The initial device selection happens outside the audio callback. One mono
    // input feeds two physical outputs; processors will operate on mono only.
    // A second input is opened by default for dry arrangement tracks.
    const auto savedDeviceState = settings.loadAudioDeviceState();
    logger.log(Logger::Level::info, "[Device] saved state: " + (savedDeviceState != nullptr
        ? savedDeviceState->toString(juce::XmlElement::TextFormat().singleLine().withoutHeader()) : juce::String("none")));
    const auto error = manager.initialise(2, 2, savedDeviceState.get(), true);
    logger.log(error.isEmpty() ? Logger::Level::info : Logger::Level::error,
               "[Device] initialise: " + (error.isEmpty() ? juce::String("ok") : error) + "; " + manager.describeCurrentSetup());
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

            logger.log(Logger::Level::info, "[Device] choosing " + juce::String(setup.sampleRate) + " Hz, buffer " + juce::String(setup.bufferSize));
            const auto setupError = manager.setAudioDeviceSetup(setup, true);
            if (setupError.isNotEmpty())
                logger.log(Logger::Level::error, "[Device] setAudioDeviceSetup: " + setupError);
        }
    }
    logger.log(Logger::Level::info, "[Device] in use: " + manager.describeCurrentSetup());
    manager.addChangeListener(this);
    manager.addAudioCallback(this);
    manager.addMidiInputDeviceCallback({}, this);
    enableMidiInputs();
}

AudioEngine::~AudioEngine()
{
    logger.log(Logger::Level::info, "Shutting down");
    manager.removeChangeListener(this);
    manager.removeAudioCallback(this);
    manager.removeMidiInputDeviceCallback({}, this);
    if (const auto state = manager.createStateXml())
        settings.saveAudioDeviceState(*state);
    manager.closeAudioDevice();
    recorder.stop();
    recorder.finaliseStoppedRecordings();
    logger.log(Logger::Level::info, "Clean exit");
    juce::Logger::setCurrentLogger(nullptr);
    logger.stop();
    settings.flush();
}

void AudioEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (! message.isActiveSense() && ! message.isMidiClock())
        keyboardCollector.addMessageToQueue(message);
}

void AudioEngine::enableMidiInputs()
{
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (! manager.isMidiInputDeviceEnabled(device.identifier))
            manager.setMidiInputDeviceEnabled(device.identifier, true);
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
    const auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    const auto onMessageThread = messageManager != nullptr && messageManager->isThisTheMessageThread();
    logger.log(onMessageThread ? Logger::Level::info : Logger::Level::warning,
               "[Device] about to start called on " + juce::String(onMessageThread ? "the message thread" : "a driver thread")
               + (callbacksInFlight.load() > 0 ? ", WHILE an audio callback is still running" : "")
               + (deviceRunning.load() ? ", without a stop since the last start" : ""));
    deviceRunning.store(false);
    const auto rate = device->getCurrentSampleRate();
    const auto block = device->getCurrentBufferSizeSamples();
    activeSampleRate.store(rate, std::memory_order_relaxed);
    filePlayer.setOutputSampleRate(rate);
    monoBuffer.setSize(1, block, false, false, true);
    stereoBuffer.setSize(2, block, false, false, true);
    keyboardCollector.reset(rate);
    rig.prepare(rate, block);
    arrangementEngine.prepare(rate, block);
    inputPeak.store(0.0f, std::memory_order_relaxed);
    outputPeak.store(0.0f, std::memory_order_relaxed);
    firstCallbackLogged.store(false, std::memory_order_relaxed);
    oversizedBlockLogged.store(false, std::memory_order_relaxed);
    concurrentCallbackLogged.store(false);
    callbackWhileStoppedLogged.store(false);
    channelMismatchLogged.store(false);
    announcedInputs.store(device->getActiveInputChannels().countNumberOfSetBits());
    announcedOutputs.store(device->getActiveOutputChannels().countNumberOfSetBits());
    logger.log(Logger::Level::info, "[Device] about to start: " + device->getTypeName() + " \"" + device->getName() + "\", "
               + juce::String(rate) + " Hz, buffer " + juce::String(block) + ", in " + device->getActiveInputChannels().toString(2)
               + " / out " + device->getActiveOutputChannels().toString(2) + ", rig prepared");
    deviceRunning.store(true);
}

void AudioEngine::audioDeviceError(const juce::String& errorMessage)
{
    logger.log(Logger::Level::error, "[Device] driver error: " + errorMessage);
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster*)
{
    logger.log(Logger::Level::info, "[Device] changed: " + manager.describeCurrentSetup());
}

void AudioEngine::audioDeviceStopped()
{
    deviceRunning.store(false);
    logger.log(callbacksInFlight.load() > 0 ? Logger::Level::warning : Logger::Level::info,
               juce::String("[Device] stopped") + (callbacksInFlight.load() > 0 ? " WHILE an audio callback is still running" : ""));
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
    // Leaves the in-flight count and the crash breadcrumb on every return path.
    struct CallbackScope
    {
        explicit CallbackScope(std::atomic<int>& counter) : count(counter), previous(counter.fetch_add(1)) {}
        ~CallbackScope() { crashlog::setAudioStage("idle"); count.fetch_sub(1); }
        std::atomic<int>& count;
        const int previous;
    } scope(callbacksInFlight);
    crashlog::setAudioStage("callback entry");

    if (scope.previous > 0 && ! concurrentCallbackLogged.exchange(true))
        logger.logRealtime(Logger::Level::error, "[Device] two audio callbacks run at the same time (e.g. an old driver thread still active)");
    if (! deviceRunning.load(std::memory_order_relaxed) && ! callbackWhileStoppedLogged.exchange(true))
        logger.logRealtime(Logger::Level::error, "[Device] audio callback outside aboutToStart/stopped (rig may be preparing)");
    if ((numInputChannels != announcedInputs.load(std::memory_order_relaxed)
         || numOutputChannels != announcedOutputs.load(std::memory_order_relaxed))
        && ! channelMismatchLogged.exchange(true))
    {
        char text[160];
        std::snprintf(text, sizeof(text), "[Device] callback channels %d in / %d out differ from announced %d / %d",
                      numInputChannels, numOutputChannels, announcedInputs.load(), announcedOutputs.load());
        logger.logRealtime(Logger::Level::warning, text);
    }

    performanceMonitor.beginBlock();
    float inPeak = 0.0f;
    float outPeak = 0.0f;
    const auto* input = numInputChannels > 0 ? inputChannelData[0] : nullptr;

    if (! firstCallbackLogged.exchange(true, std::memory_order_relaxed))
    {
        char text[160];
        std::snprintf(text, sizeof(text), "[Device] first audio callback: %d inputs, %d outputs, %d samples (prepared for %d)",
                      numInputChannels, numOutputChannels, numSamples, monoBuffer.getNumSamples());
        logger.logRealtime(Logger::Level::info, text);
    }

    if (numSamples > monoBuffer.getNumSamples())
    {
        if (! oversizedBlockLogged.exchange(true, std::memory_order_relaxed))
            logger.logRealtime(Logger::Level::warning, "[Device] driver delivered a larger block than announced; output muted");
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

    crashlog::setAudioStage("input");
    auto* mono = monoBuffer.getWritePointer(0);
    if (sourceMode.load(std::memory_order_relaxed) == SourceMode::wav)
        filePlayer.process(mono, numSamples);
    else if (input != nullptr)
        juce::FloatVectorOperations::copy(mono, input, numSamples);
    else
        juce::FloatVectorOperations::clear(mono, numSamples);
    // Re-amping: while the arrangement plays, a recorded track replaces the guitar.
    arrangementEngine.renderReampInput(mono, numSamples);

    auto* left = stereoBuffer.getWritePointer(0);
    auto* right = stereoBuffer.getWritePointer(1);
    keyboardMidi.clear();
    keyboardCollector.removeNextBlockOfMessages(keyboardMidi, numSamples);
    crashlog::setAudioStage("rig");
    inPeak = rig.processWithInputPeak(mono, left, right, numSamples, &keyboardMidi);
    crashlog::setAudioStage("recorder");
    float* stereoChannels[] { left, right };
    juce::AudioBuffer<float> stereoView(stereoChannels, 2, numSamples);
    recorder.process(stereoView);
    // Multitrack playback, input monitoring and click are added on top of the
    // live rig. Tracks recording the rig read left/right before the mix is
    // added at the end of the arrangement block.
    crashlog::setAudioStage("arrangement");
    arrangementEngine.process(inputChannelData, numInputChannels, left, right, left, right, numSamples);
    crashlog::setAudioStage("output");

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
        logger.log(Logger::Level::info, "[Device] trying type " + name);
        manager.setCurrentAudioDeviceType(name, true);
        switched = true;
        if (deviceOpen())
            return true;
    }
    return switched;
}
