#pragma once

#include "RTUtilities.h"

namespace nam::effects
{
// Feed-forward compressor with a soft knee, as on a mixing desk channel.
// The detector follows the peak level of all channels (linked), the gain
// computer works in dB and attack/release smooth the gain reduction itself,
// so the release never pumps on the waveform. Makeup gain and a dry/wet mix
// allow parallel ("New York") compression.
class CompressorProcessor final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        threshold.prepare(sampleRate, .050, -18.0f);
        ratio.prepare(sampleRate, .050, 4.0f);
        knee.prepare(sampleRate, .050, 6.0f);
        makeup.prepare(sampleRate, .050, 0.0f);
        mix.prepare(sampleRate, .030, 1.0f);
        bypass.prepare(sampleRate);
        updateTimes();
        reset();
    }
    void reset() noexcept { reductionDb = 0.0f; }

    void setThresholdDb(float v) noexcept { threshold.set(juce::jlimit(-60.0f, 0.0f, v)); }
    void setRatio(float v) noexcept { ratio.set(juce::jlimit(1.0f, 20.0f, v)); }
    void setKneeDb(float v) noexcept { knee.set(juce::jlimit(0.0f, 18.0f, v)); }
    void setAttackMs(float v) noexcept { attackMs.store(juce::jlimit(0.1f, 200.0f, v)); }
    void setReleaseMs(float v) noexcept { releaseMs.store(juce::jlimit(5.0f, 2000.0f, v)); }
    void setMakeupDb(float v) noexcept { makeup.set(juce::jlimit(0.0f, 24.0f, v)); }
    void setMix(float v) noexcept { mix.set(juce::jlimit(0.0f, 1.0f, v)); }
    void setBypassed(bool v) noexcept { bypass.setBypassed(v); }
    /** Current gain reduction in dB (<= 0), for meters. */
    [[nodiscard]] float gainReductionDb() const noexcept { return -meter.load(std::memory_order_relaxed); }

    /** Static curve: output level for an input level, both in dB. */
    static float staticCurve(float inputDb, float thresholdDb, float ratioValue, float kneeDb) noexcept
    {
        const auto over = inputDb - thresholdDb;
        if (kneeDb > 0.0f && std::abs(over) <= 0.5f * kneeDb)
        {
            const auto x = over + 0.5f * kneeDb;
            return inputDb + (1.0f / ratioValue - 1.0f) * x * x / (2.0f * kneeDb);
        }
        return over <= 0.0f ? inputDb : thresholdDb + over / ratioValue;
    }

    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        const auto channels = static_cast<int>(juce::jmin<size_t>(2, block.getNumChannels()));
        if (channels == 0 || bypass.isFullyBypassed())
        {
            meter.store(0.0f, std::memory_order_relaxed);
            return;
        }
        updateTimes();
        float deepest = 0.0f;
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            float level = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                level = std::max(level, std::abs(block.getSample(ch, static_cast<int>(i))));
            const auto inputDb = juce::Decibels::gainToDecibels(level, -120.0f);
            const auto targetDb = inputDb - staticCurve(inputDb, threshold.next(), ratio.next(), knee.next());
            // Attack while the reduction grows, release while it shrinks.
            reductionDb += (targetDb > reductionDb ? attackCoefficient : releaseCoefficient) * (targetDb - reductionDb);
            deepest = std::max(deepest, reductionDb);
            const auto gain = juce::Decibels::decibelsToGain(makeup.next() - reductionDb);
            const auto wet = mix.next() * bypass.nextWetMix();
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* p = block.getChannelPointer(static_cast<size_t>(ch));
                p[i] += wet * (p[i] * gain - p[i]);
            }
        }
        meter.store(deepest, std::memory_order_relaxed);
    }
    void process(juce::AudioBuffer<float>& b) noexcept { process(juce::dsp::AudioBlock<float>(b)); }

private:
    void updateTimes() noexcept
    {
        const auto coefficient = [this](float ms) { return static_cast<float>(1.0 - std::exp(-1.0 / (0.001 * ms * sampleRate))); };
        attackCoefficient = coefficient(attackMs.load(std::memory_order_relaxed));
        releaseCoefficient = coefficient(releaseMs.load(std::memory_order_relaxed));
    }

    double sampleRate = 48000.0;
    SmoothedParameter threshold { -18.0f }, ratio { 4.0f }, knee { 6.0f }, makeup, mix { 1.0f };
    std::atomic<float> attackMs { 10.0f }, releaseMs { 150.0f };
    float attackCoefficient = 0.0f, releaseCoefficient = 0.0f;
    float reductionDb = 0.0f;
    std::atomic<float> meter { 0.0f };
    BypassRamp bypass;
};
} // namespace nam::effects
