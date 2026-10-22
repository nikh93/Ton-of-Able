#pragma once

#include "RTUtilities.h"

namespace nam::effects
{
/** Resonant envelope-controlled band-pass filter. UI writes are atomic and
    process() stays lock-free and allocation-free. */
class AutoWahProcessor final
{
public:
    enum class Direction { down, up };

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = juce::jmax(1.0, spec.sampleRate);
        sensitivity.prepare(sampleRate, parameterRampSeconds, 0.55f);
        q.prepare(sampleRate, parameterRampSeconds, 3.0f);
        attackSeconds.prepare(sampleRate, parameterRampSeconds, 0.012f);
        releaseSeconds.prepare(sampleRate, parameterRampSeconds, 0.180f);
        minFrequency.prepare(sampleRate, parameterRampSeconds, 180.0f);
        maxFrequency.prepare(sampleRate, parameterRampSeconds, 2600.0f);
        mix.prepare(sampleRate, parameterRampSeconds, 1.0f);
        direction.prepare(sampleRate, parameterRampSeconds, 1.0f);
        bypass.prepare(sampleRate);
        reset();
    }

    void reset() noexcept { envelope = 0.0f; for (auto& state : states) state = {}; }

    // Existing API.
    void setSensitivity(float value) noexcept { sensitivity.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setResonance(float value) noexcept { q.set(juce::jlimit(0.5f, 12.0f, value)); }
    void setBypassed(bool value) noexcept { bypass.setBypassed(value); }

    // Times in seconds: fast attack tracks picking; longer release gives a vocal sweep.
    void setAttack(float seconds) noexcept { attackSeconds.set(juce::jlimit(0.001f, 0.100f, seconds)); }
    void setRelease(float seconds) noexcept { releaseSeconds.set(juce::jlimit(0.020f, 1.500f, seconds)); }
    void setDecay(float seconds) noexcept { setRelease(seconds); }

    // A useful guitar-wah range; invalid ordering is handled safely in process().
    void setMinFrequency(float hertz) noexcept { minFrequency.set(juce::jlimit(80.0f, 1800.0f, hertz)); }
    void setMaxFrequency(float hertz) noexcept { maxFrequency.set(juce::jlimit(300.0f, 8000.0f, hertz)); }
    void setFrequencyRange(float minimumHertz, float maximumHertz) noexcept { setMinFrequency(minimumHertz); setMaxFrequency(maximumHertz); }
    void setMix(float value) noexcept { mix.set(juce::jlimit(0.0f, 1.0f, value)); }

    void setDirection(Direction value) noexcept { direction.set(value == Direction::up ? 1.0f : 0.0f); }
    void setDirectionUp(bool shouldSweepUp) noexcept { setDirection(shouldSweepUp ? Direction::up : Direction::down); }
    void setDirectionDown(bool shouldSweepDown) noexcept { setDirection(shouldSweepDown ? Direction::down : Direction::up); }

    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        constexpr float pi = juce::MathConstants<float>::pi;
        const auto channelCount = block.getNumChannels();
        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            float inputPeak = 0.0f;
            for (size_t channel = 0; channel < channelCount; ++channel)
                inputPeak = juce::jmax(inputPeak, std::abs(block.getChannelPointer(channel)[sample]));

            const float attack = attackSeconds.next(), release = releaseSeconds.next();
            const float time = inputPeak > envelope ? attack : release;
            const float coefficient = 1.0f - std::exp(-1.0f / (juce::jmax(0.001f, time) * static_cast<float>(sampleRate)));
            envelope += coefficient * (inputPeak - envelope);

            // A logarithmic sensitivity range also tracks quiet guitar signals.
            const float detectorGain = std::pow(10.0f, -0.5f + 2.5f * sensitivity.next());
            const float amount = 1.0f - std::exp(-envelope * detectorGain);
            const float low = minFrequency.next();
            const float high = juce::jlimit(low + 50.0f, 8000.0f, maxFrequency.next());
            const float sweepUp = low * std::pow(high / low, amount);
            const float sweepDown = low * std::pow(high / low, 1.0f - amount);
            const float cutoff = sweepDown + direction.next() * (sweepUp - sweepDown);
            const float g = std::tan(pi * juce::jmin(cutoff, static_cast<float>(sampleRate) * .45f) / static_cast<float>(sampleRate));
            const float resonance = q.next();
            const float k = 1.0f / resonance;
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float wetMix = bypass.nextWetMix() * mix.next();

            for (size_t channel = 0; channel < channelCount; ++channel)
            {
                auto* samples = block.getChannelPointer(channel);
                auto& state = states[channel < std::size(states) ? channel : 0];
                const float dry = samples[sample];
                // Topology-preserving SVF: stable across the full Q/frequency range.
                const float v3 = dry - state.low;
                const float band = a1 * state.band + g * a1 * v3;
                const float lowPass = state.low + g * band;
                state.band = 2.0f * band - state.band;
                state.low = 2.0f * lowPass - state.low;
                const float effected = band * (2.0f / std::sqrt(resonance));
                samples[sample] = dry + wetMix * (effected - dry);
            }
        }
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept { process(juce::dsp::AudioBlock<float>(buffer)); }

private:
    struct State { float low {}, band {}; };
    static constexpr double parameterRampSeconds = 0.030;
    double sampleRate = 44100.0;
    float envelope = 0.0f;
    State states[2];
    SmoothedParameter sensitivity, q, attackSeconds, releaseSeconds, minFrequency, maxFrequency, mix, direction;
    BypassRamp bypass;
};
} // namespace nam::effects
