#pragma once

#include <atomic>

#include <juce_audio_utils/juce_audio_utils.h>

#include "RigChain.h"
#include "config/AppSettings.h"
#include "daw/ArrangementEngine.h"
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
    void loadNamAsync(const juce::File& modelFile) { rig.loadNamAsync(modelFile); }
    juce::String activeNamName() const { return rig.activeNamName(); }
    void setNamPedal(const juce::String& filePath, bool bypass, float inputDb, float outputDb) { rig.setNamPedal(filePath, bypass, inputDb, outputDb); }
    juce::String activeNamPedalName() const { return rig.activeNamPedalName(); }
    juce::String namPedalError() const { return rig.namPedalError(); }
    juce::Result loadIr(const juce::File& file) { return rig.loadIr(file); }
    juce::String activeIrName() const { return rig.activeIrName(); }
    juce::Result loadWav(const juce::File& file) { return filePlayer.load(file); }
    void setSourceMode(SourceMode mode) noexcept { sourceMode.store(mode, std::memory_order_release); }
    SourceMode getSourceMode() const noexcept { return sourceMode.load(std::memory_order_acquire); }
    FilePlayer& player() noexcept { return filePlayer; }
    juce::Result startRecording(const juce::File& target);
    void stopRecording() noexcept { recorder.stop(); }
    bool isRecording() const noexcept { return recorder.isRecording(); }
    void setInputGainDb(float value) noexcept { rig.setInputGainDb(value); }
    void setOutputGainDb(float value) noexcept { rig.setOutputGainDb(value); }
    void setBassDb(float value) noexcept { rig.setBassDb(value); }
    void setMidDb(float value) noexcept { rig.setMidDb(value); }
    void setTrebleDb(float value) noexcept { rig.setTrebleDb(value); }
    void setNamBypassed(bool value) noexcept { rig.setNamBypassed(value); }
    void setDriveBypassed(bool value) noexcept { rig.setDriveBypassed(value); }
    void setIrBypassed(bool value) noexcept { rig.setIrBypassed(value); }
    void setAutoWahBypassed(bool value) noexcept { rig.setAutoWahBypassed(value); }
    void setChorusBypassed(bool value) noexcept { rig.setChorusBypassed(value); }
    void setDelayBypassed(bool value) noexcept { rig.setDelayBypassed(value); }
    void setReverbBypassed(bool value) noexcept { rig.setReverbBypassed(value); }
    void setGraphicEqBypassed(bool value) noexcept { rig.setGraphicEqBypassed(value); }
    void setGraphicEqBandDb(int band, float db) noexcept { rig.setGraphicEqBandDb(band, db); }
    void setPitch(bool bypassed, float semitones, float mix, bool formant, bool highQuality, bool analogOctave) noexcept { rig.setPitch(bypassed, semitones, mix, formant, highQuality, analogOctave); }
    int pitchLatencySamples() const noexcept { return rig.pitchLatencySamples(); }
    void applyEffectState(const SoundState& state) { rig.applyEffectState(state); }
    void setWahPosition(float position) noexcept { rig.setWahPosition(position); }
    MeterSnapshot meterSnapshot() const noexcept;
    double estimatedLatencyMilliseconds() const;
    PerformanceMonitor::Snapshot performanceSnapshot() const noexcept { return performanceMonitor.snapshot(); }
    std::vector<Logger::Entry> recentLogEntries() const { return logger.recentEntries(); }
    const std::atomic<float>& inputPeakSource() const noexcept { return inputPeak; }
    const std::atomic<float>& outputPeakSource() const noexcept { return outputPeak; }
    juce::String lastError() const;
    Vst3Slot& prePlugin() noexcept { return rig.prePlugin(); }
    /** Guitar to MIDI to synth (live rig). */
    GuitarSynth& guitarSynth() noexcept { return guitarSynthHost; }
    /** Opens every MIDI input so keyboards can play the synth. Message thread. */
    void enableMidiInputs();
    Vst3Slot& postPlugin() noexcept { return rig.postPlugin(); }
    /** Processing latency of the live rig (IR, drive oversampling, VST3). */
    int rigLatencySamples() const { return rig.latencySamples(); }
    daw::ArrangementEngine& arrangement() noexcept { return arrangementEngine; }

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
    GuitarSynth guitarSynthHost; // before rig: the rig points to it
    RigChain rig;
    juce::AudioBuffer<float> monoBuffer;
    juce::AudioBuffer<float> stereoBuffer;
    std::atomic<double> activeSampleRate { 48000.0 };
    std::atomic<SourceMode> sourceMode { SourceMode::live };
    FilePlayer filePlayer;
    Recorder recorder;
    daw::ArrangementEngine arrangementEngine;
    PerformanceMonitor performanceMonitor;
    Logger logger;
    juce::String lastDeviceError;
    juce::CriticalSection errorLock;
};
