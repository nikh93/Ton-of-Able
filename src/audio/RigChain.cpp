#include "RigChain.h"

#include <algorithm>

#include "presets/PresetManager.h"

RigChain::RigChain() = default;
RigChain::~RigChain() = default;

void RigChain::prepare(double sampleRate, int maximumBlockSize)
{
    maximumBlock = std::max(1, maximumBlockSize);
    namProcessor.setProcessingFormat(sampleRate, maximumBlock);
    namPedalProcessor.prepare(sampleRate, maximumBlock);
    namDryBuffer.setSize(1, maximumBlock, false, false, true);
    guitarDryBuffer.setSize(1, maximumBlock, false, false, true);
    if (guitarSynth != nullptr)
        guitarSynth->prepare(sampleRate, maximumBlock);
    preVst3.prepare(sampleRate, maximumBlock);
    postVst3.prepare(sampleRate, maximumBlock);
    const juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(maximumBlock), 1 };
    eqProcessor.prepare(spec);
    graphicEq.prepare(spec);
    pitchShifter.prepare(sampleRate, maximumBlock);
    driveProcessor.prepare(spec);
    compressor.prepare(spec);
    wahPedal.prepare(spec);
    namBypassRamp.prepare(spec.sampleRate, false);
    namInputTrim.prepare(spec.sampleRate, .030, 0.f);
    irProcessor.prepare(spec);
    autoWahProcessor.prepare(spec);
    chorusProcessor.prepare(spec);
    delayProcessor.prepare(spec);
    reverbProcessor.prepare(spec);
    inputGain.reset(spec.sampleRate, 0.020);
    outputGain.reset(spec.sampleRate, 0.020);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputGainDb.load(std::memory_order_relaxed)));
}

int RigChain::latencySamples() const
{
    return pitchShifter.latencySamples() + irProcessor.latencySamples() + driveProcessor.latencySamples()
         + preVst3.latencySamples() + postVst3.latencySamples();
}

void RigChain::applyEffectState(const SoundState& state)
{
    // Path loading is explicit via setNamPedal when selecting/restoring a pedal.
    namPedalProcessor.setControls(state.namPedalBypassed, state.namPedalInputDb, state.namPedalOutputDb);
    namInputTrim.set(juce::jlimit(-18.f, 18.f, state.namInputTrimDb));
    setInputGainDb(state.inputGainDb); setOutputGainDb(state.outputGainDb);
    setBassDb(state.bassDb); setMidDb(state.midDb); setTrebleDb(state.trebleDb);
    setNamBypassed(state.namBypassed); setIrBypassed(state.irBypassed);
    setDriveBypassed(state.driveBypassed); driveProcessor.setType(static_cast<nam::effects::DriveProcessor::DriveType>(juce::jlimit(0, 3, state.driveType))); driveProcessor.setDrive(state.driveAmount); driveProcessor.setTone(state.driveTone); driveProcessor.setWarmth(state.driveWarmth); driveProcessor.setPresence(state.drivePresence); driveProcessor.setSustain(state.driveSustain); driveProcessor.setBias(state.driveBias); driveProcessor.setLevel(state.driveLevel); driveProcessor.setMix(state.driveMix);
    setAutoWahBypassed(state.autoWahBypassed); autoWahProcessor.setSensitivity(state.autoWahSensitivity); autoWahProcessor.setAttack(state.autoWahAttackMs * 0.001f); autoWahProcessor.setRelease(state.autoWahReleaseMs * 0.001f); autoWahProcessor.setMinFrequency(state.autoWahMinimumHz); autoWahProcessor.setMaxFrequency(state.autoWahMaximumHz); autoWahProcessor.setResonance(state.autoWahQ); autoWahProcessor.setMix(state.autoWahMix); autoWahProcessor.setDirectionUp(state.autoWahUpward);
    setChorusBypassed(state.chorusBypassed); chorusProcessor.setRateHz(state.chorusRateHz); chorusProcessor.setDepth(juce::jlimit(0.0f, 1.0f, state.chorusDepthMs / 20.0f)); chorusProcessor.setCenterDelayMs(state.chorusDelayMs); chorusProcessor.setFeedback(state.chorusFeedback); chorusProcessor.setMix(state.chorusMix);
    setDelayBypassed(state.delayBypassed); delayProcessor.setTimeSeconds(state.delayTimeMs * 0.001f); delayProcessor.setFeedback(state.delayFeedback); delayProcessor.setMix(state.delayMix); delayProcessor.setLowCutHz(state.delayLowCutHz); delayProcessor.setHighCutHz(state.delayHighCutHz);
    setReverbBypassed(state.reverbBypassed); reverbProcessor.setSize(state.reverbSize); reverbProcessor.setDecay(state.reverbDecay); reverbProcessor.setDamping(state.reverbDamping); reverbProcessor.setPreDelay(state.reverbPreDelayMs); reverbProcessor.setMix(state.reverbMix); reverbProcessor.setLowCut(state.reverbLowCutHz); reverbProcessor.setHighCut(state.reverbHighCutHz);
    setGraphicEqBypassed(state.graphicEqBypassed);
    for (int band = 0; band < static_cast<int>(state.graphicEqDb.size()); ++band)
        setGraphicEqBandDb(band, state.graphicEqDb[static_cast<size_t>(band)]);
    setPitch(state.pitchBypassed, state.pitchSemitones, state.pitchMix, state.pitchFormantPreserved, state.pitchHighQuality, state.pitchAnalogOctave);
    setWahPedal(state.pedalWahBypassed, state.pedalWahPosition, state.pedalWahHeelHz, state.pedalWahToeHz, state.pedalWahQ, state.pedalWahMix);
    setCompressor(state.compressorBypassed, state.compressorPreAmp, state.compressorThresholdDb, state.compressorRatio, state.compressorAttackMs,
                  state.compressorReleaseMs, state.compressorKneeDb, state.compressorMakeupDb, state.compressorMix);
}

float RigChain::processWithInputPeak(float* mono, float* left, float* right, int numSamples) noexcept
{
    float inPeak = 0.0f;
    if (numSamples > maximumBlock)
    {
        // Never allocate here; a block larger than announced is dropped.
        juce::FloatVectorOperations::clear(left, numSamples);
        juce::FloatVectorOperations::clear(right, numSamples);
        return inPeak;
    }

    inputGain.setTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDb.load(std::memory_order_relaxed)));
    for (int sample = 0; sample < numSamples; ++sample)
    {
        mono[sample] *= inputGain.getNextValue();
        inPeak = std::max(inPeak, std::abs(mono[sample]));
    }

    // The guitar-to-MIDI tracker hears the plain guitar, before any effect.
    if (guitarSynth != nullptr && guitarSynth->isEnabled())
        juce::FloatVectorOperations::copy(guitarDryBuffer.getWritePointer(0), mono, numSamples);
    // Like a pitch pedal in front of the amp: shift first, then drive and NAM.
    pitchShifter.process(mono, numSamples);
    // Stack-backed channel view: process exactly the current callback length.
    juce::AudioBuffer<float> activeBuffer(&mono, 1, numSamples);
    // Compressor either as a pedal in front of the amp or on the miked signal.
    const auto compressPreAmp = compressorPreAmp.load(std::memory_order_relaxed);
    if (compressPreAmp)
        compressor.process(activeBuffer);
    // Pedalboard order: compressor, wah, then drive into the amp.
    wahPedal.process(activeBuffer);
    driveProcessor.process(activeBuffer);
    preVst3.process(activeBuffer);
    namPedalProcessor.process(mono, numSamples);
    // The independent NAM amp follows the NAM pedal. It is bypassed until the loader
    // thread has supplied a prepared model.
    auto* namDry = namDryBuffer.getWritePointer(0);
    juce::FloatVectorOperations::copy(namDry, mono, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
        mono[sample] *= juce::Decibels::decibelsToGain(namInputTrim.next());
    namProcessor.process(mono, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto mix = namBypassRamp.nextWetMix();
        mono[sample] = namDry[sample] + mix * (mono[sample] - namDry[sample]);
    }
    irProcessor.process(activeBuffer);
    eqProcessor.process(activeBuffer);
    graphicEq.process(activeBuffer);
    if (! compressPreAmp)
        compressor.process(activeBuffer);
    autoWahProcessor.process(activeBuffer);
    chorusProcessor.process(activeBuffer);
    delayProcessor.process(activeBuffer);
    reverbProcessor.process(activeBuffer);
    juce::FloatVectorOperations::copy(left, mono, numSamples);
    juce::FloatVectorOperations::copy(right, mono, numSamples);
    float* stereoChannels[] { left, right };
    juce::AudioBuffer<float> stereoView(stereoChannels, 2, numSamples);
    if (guitarSynth != nullptr)
        guitarSynth->process(guitarDryBuffer.getReadPointer(0), left, right, numSamples);
    postVst3.process(stereoView);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float gain = outputGain.getNextValue();
        left[sample] *= gain; right[sample] *= gain;
    }
    return inPeak;
}
