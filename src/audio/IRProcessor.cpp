#include "IRProcessor.h"

void IRProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize), false, false, true);
    convolution.prepare(spec);
    bypassRamp.prepare(spec.sampleRate, true);
    prepared = true;
}

void IRProcessor::reset() noexcept
{
    convolution.reset();
    dryBuffer.clear();
}

juce::Result IRProcessor::loadAsync(const juce::File& irFile)
{
    if (!irFile.existsAsFile())
        return juce::Result::fail("IR-Datei nicht gefunden: " + irFile.getFullPathName());
    if (!prepared)
        return juce::Result::fail("Audio-Engine ist noch nicht bereit für eine IR.");

    // JUCE queues decoding/preprocessing internally. This call is wait-free and
    // accepts WAV as well as other JUCE-supported audio files; the library UI
    // deliberately filters imports to WAV.
    convolution.loadImpulseResponse(irFile,
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::yes,
                                    0,
                                    juce::dsp::Convolution::Normalise::yes);
    bypassRamp.setBypassed(false);
    {
        const juce::ScopedLock lock(stateLock);
        name = irFile.getFileNameWithoutExtension();
        error.clear();
    }
    return juce::Result::ok();
}

void IRProcessor::process(juce::AudioBuffer<float>& monoBuffer) noexcept
{
    const auto samples = monoBuffer.getNumSamples();
    if (monoBuffer.getNumChannels() == 0 || samples <= 0 || samples > dryBuffer.getNumSamples())
        return;

    auto* dry = dryBuffer.getWritePointer(0);
    auto* wet = monoBuffer.getWritePointer(0);
    juce::FloatVectorOperations::copy(dry, wet, samples);

    auto block = juce::dsp::AudioBlock<float>(monoBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolution.process(context);

    for (int sample = 0; sample < samples; ++sample)
    {
        const auto mix = bypassRamp.nextWetMix();
        wet[sample] = dry[sample] + mix * (wet[sample] - dry[sample]);
    }
}

juce::String IRProcessor::loadedName() const
{
    const juce::ScopedLock lock(stateLock);
    return name;
}

juce::String IRProcessor::lastError() const
{
    const juce::ScopedLock lock(stateLock);
    return error;
}
