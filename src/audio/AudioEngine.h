#pragma once

#include <atomic>

#include <juce_audio_utils/juce_audio_utils.h>

#include "NamProcessor.h"
#include "NamPedalProcessor.h"
#include "IRProcessor.h"
#include "plugins/Vst3Slot.h"
#include "audio/effects/Effects.h"
#include "config/AppSettings.h"
#include "diagnostics/Logger.h"
#include "diagnostics/PerformanceMonitor.h"
#include "playback/FilePlayer.h"
#include "playback/Recorder.h"

struct SoundState;

class AudioEngine final : private juce::AudioIODeviceCallback
{
public:
    enum class SourceMode : unsigned char { live, wav };
    struct MeterSnapshot
    {
        float inputPeak = 0.0f;
        float outputPeak = 0.0f;
        double cpuLoad = 0.0;
    };

    AudioEngine();
    ~AudioEngine() override;

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    juce::AudioDeviceManager& deviceManager() noexcept { return manager; }
    const juce::AudioDeviceManager& deviceManager() const noexcept { return manager; }
    AppSettings& appSettings() noexcept { return settings; }
    void loadNamAsync(const juce::File& modelFile) { namProcessor.loadAsync(modelFile); }
    juce::String activeNamName() const { return namProcessor.activeModelName(); }
    void setNamPedal(const juce::String& filePath, bool bypass, float inputDb, float outputDb);
    juce::String activeNamPedalName() const { return namPedalProcessor.name(); }
    juce::String namPedalError() const { return namPedalProcessor.lastError(); }
    juce::Result loadIr(const juce::File& file) { return irProcessor.loadAsync(file); }
    juce::String activeIrName() const { return irProcessor.loadedName(); }
    juce::Result loadWav(const juce::File& file) { return filePlayer.load(file); }
    void setSourceMode(SourceMode mode) noexcept { sourceMode.store(mode, std::memory_order_release); }
    SourceMode getSourceMode() const noexcept { return sourceMode.load(std::memory_order_acquire); }
    FilePlayer& player() noexcept { return filePlayer; }
    juce::Result startRecording(const juce::File& target);
    void stopRecording() noexcept { recorder.stop(); }
    bool isRecording() const noexcept { return recorder.isRecording(); }
    void setInputGainDb(float value) noexcept { inputGainDb.store(juce::jlimit(-24.0f, 24.0f, value), std::memory_order_relaxed); }
    void setOutputGainDb(float value) noexcept { outputGainDb.store(juce::jlimit(-24.0f, 24.0f, value), std::memory_order_relaxed); }
    void setBassDb(float value) noexcept { eqProcessor.setLowGainDb(value); }
    void setMidDb(float value) noexcept { eqProcessor.setMidGainDb(value); }
    void setTrebleDb(float value) noexcept { eqProcessor.setHighGainDb(value); }
    void setNamBypassed(bool value) noexcept { namBypassRamp.setBypassed(value); }
    void setDriveBypassed(bool value) noexcept { driveProcessor.setBypassed(value); }
    void setIrBypassed(bool value) noexcept { irProcessor.setBypassed(value); }
    void setAutoWahBypassed(bool value) noexcept { autoWahProcessor.setBypassed(value); }
    void setChorusBypassed(bool value) noexcept { chorusProcessor.setBypassed(value); }
    void setDelayBypassed(bool value) noexcept { delayProcessor.setBypassed(value); }
    void setReverbBypassed(bool value) noexcept { reverbProcessor.setBypassed(value); }
    void applyEffectState(const SoundState& state);
    MeterSnapshot meterSnapshot() const noexcept;
    double estimatedLatencyMilliseconds() const;
    PerformanceMonitor::Snapshot performanceSnapshot() const noexcept { return performanceMonitor.snapshot(); }
    std::vector<Logger::Entry> recentLogEntries() const { return logger.recentEntries(); }
    const std::atomic<float>& inputPeakSource() const noexcept { return inputPeak; }
    const std::atomic<float>& outputPeakSource() const noexcept { return outputPeak; }
    juce::String lastError() const;
    Vst3Slot& prePlugin() noexcept { return preVst3; }
    Vst3Slot& postPlugin() noexcept { return postVst3; }

private:
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override;

    bool preferLowLatencyDeviceType(bool firstStart);

    AppSettings settings;
    juce::AudioDeviceManager manager;
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    NamProcessor namProcessor;
    NamPedalProcessor namPedalProcessor;
    IRProcessor irProcessor;
    juce::AudioBuffer<float> monoBuffer;
    juce::AudioBuffer<float> namDryBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    Vst3Slot preVst3, postVst3;
    std::atomic<double> activeSampleRate { 48000.0 };
    nam::effects::EQProcessor eqProcessor;
    nam::effects::DriveProcessor driveProcessor;
    nam::effects::AutoWahProcessor autoWahProcessor;
    nam::effects::ChorusProcessor chorusProcessor;
    nam::effects::DelayProcessor delayProcessor;
    nam::effects::ReverbProcessor reverbProcessor;
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { 0.0f };
    nam::effects::BypassRamp namBypassRamp;
    nam::effects::SmoothedParameter namInputTrim { 0.f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    std::atomic<SourceMode> sourceMode { SourceMode::live };
    FilePlayer filePlayer;
    Recorder recorder;
    PerformanceMonitor performanceMonitor;
    Logger logger;
    juce::String lastDeviceError;
    juce::CriticalSection errorLock;
};
