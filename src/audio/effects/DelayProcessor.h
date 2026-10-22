#pragma once

#include "RTUtilities.h"

namespace nam::effects
{
class DelayProcessor final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        size = (int) std::ceil(sampleRate * 2.1) + 2;
        line.setSize((int) juce::jmin<size_t>(2, spec.numChannels), size, false, false, true);

        // The smooth read-position glide prevents discontinuities in the wet path
        // when the user changes the delay time.
        time.prepare(sampleRate, 0.100, 0.350f);
        feedback.prepare(sampleRate, 0.050, 0.30f);
        mix.prepare(sampleRate, 0.030, 0.25f);
        lowCut.prepare(sampleRate, 0.040, 80.0f);
        highCut.prepare(sampleRate, 0.040, 7500.0f);
        bypass.prepare(sampleRate);
        reset();
    }

    void reset() noexcept
    {
        line.clear();
        write = 0;
        for (auto& state : lowCutState) state = 0.0f;
        for (auto& state : dampingState) state = 0.0f;
    }

    void setTimeSeconds(float value) noexcept { time.set(juce::jlimit(0.010f, 2.0f, value)); }
    void setFeedback(float value) noexcept { feedback.set(juce::jlimit(0.0f, 0.92f, value)); }
    void setMix(float value) noexcept { mix.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setLowCutHz(float value) noexcept { lowCut.set(juce::jlimit(20.0f, 2000.0f, value)); }
    void setHighCutHz(float value) noexcept { highCut.set(juce::jlimit(500.0f, 20000.0f, value)); }
    void setDampingHz(float value) noexcept { setHighCutHz(value); }
    void setBypassed(bool value) noexcept { bypass.setBypassed(value); }

    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        const auto channels = (int) juce::jmin<size_t>(2, block.getNumChannels());
        if (channels == 0 || size == 0)
            return;

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto delaySamples = time.next() * (float) sampleRate;
            const auto readA = (write - (int) delaySamples + size) % size;
            const auto readB = (readA - 1 + size) % size;
            const auto fraction = delaySamples - std::floor(delaySamples);
            const auto feedbackAmount = feedback.next();
            const auto wetMix = mix.next() * bypass.nextWetMix();
            const auto lowHz = lowCut.next();
            const auto highHz = juce::jmax(lowHz + 10.0f, highCut.next());
            const auto lowCoefficient = onePoleCoefficient(lowHz);
            const auto highCoefficient = onePoleCoefficient(highHz);

            for (int channel = 0; channel < channels; ++channel)
            {
                auto* samples = block.getChannelPointer((size_t) channel);
                const auto dry = samples[i];
                const auto delayed = line.getSample(channel, readA) * (1.0f - fraction)
                                   + line.getSample(channel, readB) * fraction;

                // The processor is mono; duplicate host channels only retain their
                // own fixed state so a duplicated mono output remains phase-aligned.
                lowCutState[(size_t) channel] += lowCoefficient * (delayed - lowCutState[(size_t) channel]);
                const auto highPassed = delayed - lowCutState[(size_t) channel];
                dampingState[(size_t) channel] += highCoefficient * (highPassed - dampingState[(size_t) channel]);
                line.setSample(channel, write, dry + dampingState[(size_t) channel] * feedbackAmount);
                samples[i] = dry + wetMix * (delayed - dry);
            }

            write = (write + 1) % size;
        }
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept { process(juce::dsp::AudioBlock<float>(buffer)); }

private:
    float onePoleCoefficient(float cutoffHz) const noexcept
    {
        return 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
    }

    double sampleRate = 44100.0;
    int size = 0;
    int write = 0;
    juce::AudioBuffer<float> line;
    SmoothedParameter time, feedback, mix, lowCut, highCut;
    BypassRamp bypass;
    float lowCutState[2] {};
    float dampingState[2] {};
};
} // namespace nam::effects
