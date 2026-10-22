#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

#include "NamProcessor.h"
#include "NamPedalProcessor.h"
#include "IRProcessor.h"
#include "plugins/Vst3Slot.h"
#include "audio/effects/Effects.h"
#include "audio/effects/GraphicEQProcessor.h"
#include "audio/effects/LivePitchShifter.h"
#include "daw/Arrangement.h"

struct SoundState;

/** The complete guitar chain: input gain, live pitch shifter, [compressor], wah pedal, drive, pre-VST3, NAM pedal,
    NAM amp, cabinet IR, 3-band EQ, graphic EQ, [compressor], Auto-Wah, Chorus, Delay, Reverb,
    post-VST3 (stereo) and output gain.

    Used once for the live rig and once per arrangement track that has its
    own effects. Models and IRs load off the audio thread; process() does not
    allocate (third-party VST3 code excepted). */
class RigChain : public daw::TrackProcessor
{
public:
    RigChain();
    ~RigChain() override;
    RigChain(const RigChain&) = delete;
    RigChain& operator=(const RigChain&) = delete;

    // daw::TrackProcessor
    void prepare(double sampleRate, int maximumBlockSize) override;
    void process(float* mono, float* left, float* right, int numSamples) noexcept override { processWithInputPeak(mono, left, right, numSamples); }
    [[nodiscard]] int latencySamples() const override;

    /** Same as process(); returns the input peak after the input gain. */
    float processWithInputPeak(float* mono, float* left, float* right, int numSamples) noexcept;

    // Message thread.
    void loadNamAsync(const juce::File& modelFile) { namProcessor.loadAsync(modelFile); }
    [[nodiscard]] juce::String activeNamName() const { return namProcessor.activeModelName(); }
    void setNamPedal(const juce::String& filePath, bool bypass, float inputDb, float outputDb) { namPedalProcessor.set(filePath, bypass, inputDb, outputDb); }
    [[nodiscard]] juce::String activeNamPedalName() const { return namPedalProcessor.name(); }
    [[nodiscard]] juce::String namPedalError() const { return namPedalProcessor.lastError(); }
    juce::Result loadIr(const juce::File& file) { return irProcessor.loadAsync(file); }
    [[nodiscard]] juce::String activeIrName() const { return irProcessor.loadedName(); }
    void applyEffectState(const SoundState& state);

    // Lock-free parameter setters.
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
    void setGraphicEqBypassed(bool value) noexcept { graphicEq.setBypassed(value); }
    void setCompressor(bool bypassed, bool preAmp, float thresholdDb, float ratio, float attackMs, float releaseMs,
                       float kneeDb, float makeupDb, float mix) noexcept
    {
        compressor.setThresholdDb(thresholdDb); compressor.setRatio(ratio); compressor.setAttackMs(attackMs);
        compressor.setReleaseMs(releaseMs); compressor.setKneeDb(kneeDb); compressor.setMakeupDb(makeupDb);
        compressor.setMix(mix); compressorPreAmp.store(preAmp); compressor.setBypassed(bypassed);
    }
    [[nodiscard]] float compressorGainReductionDb() const noexcept { return compressor.gainReductionDb(); }
    void setWahPedal(bool bypassed, float position, float heelHz, float toeHz, float q, float mix) noexcept
    {
        wahPedal.setPosition(position); wahPedal.setHeelHz(heelHz); wahPedal.setToeHz(toeHz);
        wahPedal.setQ(q); wahPedal.setMix(mix); wahPedal.setBypassed(bypassed);
    }
    /** Lock-free, any rate (e.g. from the mouse): 0 heel ... 1 toe. */
    void setWahPosition(float position) noexcept { wahPedal.setPosition(position); }
    void setGraphicEqBandDb(int band, float db) noexcept { graphicEq.setBandGainDb(band, db); }
    void setPitch(bool bypassed, float semitones, float mix, bool formantPreserved, bool highQuality) noexcept
    {
        pitchShifter.setSemitones(semitones);
        pitchShifter.setMix(mix);
        pitchShifter.setFormantPreserved(formantPreserved);
        pitchShifter.setMode(highQuality ? nam::effects::LivePitchShifter::Mode::highQuality : nam::effects::LivePitchShifter::Mode::lowLatency);
        pitchShifter.setBypassed(bypassed);
    }
    [[nodiscard]] int pitchLatencySamples() const noexcept { return pitchShifter.latencySamples(); }

    Vst3Slot& prePlugin() noexcept { return preVst3; }
    Vst3Slot& postPlugin() noexcept { return postVst3; }

private:
    NamProcessor namProcessor;
    NamPedalProcessor namPedalProcessor;
    IRProcessor irProcessor;
    Vst3Slot preVst3, postVst3;
    juce::AudioBuffer<float> namDryBuffer;
    nam::effects::EQProcessor eqProcessor;
    nam::effects::GraphicEQProcessor graphicEq;
    nam::effects::LivePitchShifter pitchShifter;
    nam::effects::DriveProcessor driveProcessor;
    nam::effects::CompressorProcessor compressor;
    nam::effects::WahPedalProcessor wahPedal;
    std::atomic<bool> compressorPreAmp { false };
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
    int maximumBlock = 0;
};
