#pragma once

#include "RTUtilities.h"

namespace nam::effects
{
// Classic wah pedal: a resonant band-pass whose centre frequency follows the
// pedal position, exponentially between heel (dark) and toe (bright), like a
// Cry Baby's inductor sweep. The position is a plain atomic, so the mouse
// (or a knob) can move it at any rate; it is smoothed per sample here.
// Topology-preserving state-variable filter: stable under fast sweeps.
class WahPedalProcessor final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = juce::jmax(1.0, spec.sampleRate);
        heel.prepare(sampleRate, .050, 350.0f);
        toe.prepare(sampleRate, .050, 2200.0f);
        q.prepare(sampleRate, .050, 6.0f);
        mix.prepare(sampleRate, .030, 1.0f);
        bypass.prepare(sampleRate);
        positionSmoothed.reset(sampleRate, 0.025);
        positionSmoothed.setCurrentAndTargetValue(position.load());
        reset();
    }
    void reset() noexcept { for (auto& s : states) s = {}; }

    /** 0 = heel (dark), 1 = toe (bright). */
    void setPosition(float value) noexcept { position.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed); }
    [[nodiscard]] float getPosition() const noexcept { return position.load(std::memory_order_relaxed); }
    void setHeelHz(float hz) noexcept { heel.set(juce::jlimit(100.0f, 1500.0f, hz)); }
    void setToeHz(float hz) noexcept { toe.set(juce::jlimit(500.0f, 6000.0f, hz)); }
    void setQ(float value) noexcept { q.set(juce::jlimit(0.7f, 15.0f, value)); }
    void setMix(float value) noexcept { mix.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setBypassed(bool value) noexcept { bypass.setBypassed(value); }

    /** Centre frequency for a pedal position (exponential sweep). */
    static float centreFrequency(float pedal, float heelHz, float toeHz) noexcept
    {
        return heelHz * std::pow(toeHz / heelHz, juce::jlimit(0.0f, 1.0f, pedal));
    }

    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        if (bypass.isFullyBypassed())
            return;
        const auto channels = juce::jmin<size_t>(2, block.getNumChannels());
        positionSmoothed.setTargetValue(position.load(std::memory_order_relaxed));
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto frequency = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45),
                                                centreFrequency(positionSmoothed.getNextValue(), heel.next(), toe.next()));
            const auto g = std::tan(juce::MathConstants<float>::pi * frequency / static_cast<float>(sampleRate));
            const auto k = 1.0f / q.next();
            const auto a1 = 1.0f / (1.0f + g * (g + k));
            const auto wet = mix.next() * bypass.nextWetMix();
            for (size_t ch = 0; ch < channels; ++ch)
            {
                auto& s = states[ch];
                auto* p = block.getChannelPointer(ch);
                const auto v0 = p[i];
                const auto v1 = a1 * (s.ic1 + g * (v0 - s.ic2));
                const auto v2 = s.ic2 + g * v1;
                s.ic1 = 2.0f * v1 - s.ic1;
                s.ic2 = 2.0f * v2 - s.ic2;
                // Band-pass normalised to unity at the peak, plus the usual wah boost (+6 dB).
                const auto band = 2.0f * k * v1;
                p[i] = v0 + wet * (band - v0);
            }
        }
    }
    void process(juce::AudioBuffer<float>& b) noexcept { process(juce::dsp::AudioBlock<float>(b)); }

private:
    struct State { float ic1 = 0.0f, ic2 = 0.0f; };
    double sampleRate = 48000.0;
    std::atomic<float> position { 0.5f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> positionSmoothed;
    SmoothedParameter heel { 350.0f }, toe { 2200.0f }, q { 6.0f }, mix { 1.0f };
    BypassRamp bypass;
    State states[2];
};
} // namespace nam::effects
