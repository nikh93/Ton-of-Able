#pragma once

#include <atomic>

#include <juce_dsp/juce_dsp.h>

#include "audio/effects/RTUtilities.h"

// Mono cabinet IR processor. JUCE's Convolution asynchronously preprocesses
// loaded WAV data; the audio callback only calls process() on prepared buffers.
class IRProcessor final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;

    juce::Result loadAsync(const juce::File& irFile);
    void setBypassed(bool bypassed) noexcept { bypassRamp.setBypassed(bypassed); }
    void process(juce::AudioBuffer<float>& monoBuffer) noexcept;

    int latencySamples() const noexcept { return convolution.getLatency(); }
    int currentIrSize() const noexcept { return convolution.getCurrentIRSize(); }
    juce::String loadedName() const;
    juce::String lastError() const;

private:
    juce::dsp::Convolution convolution;
    juce::AudioBuffer<float> dryBuffer;
    nam::effects::BypassRamp bypassRamp;
    juce::CriticalSection stateLock;
    juce::String name;
    juce::String error;
    bool prepared = false;
};
