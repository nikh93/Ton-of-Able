#pragma once

#include <array>

#include "RTUtilities.h"

namespace nam::effects
{
// Ten-band octave graphic EQ (ISO centres 31.5 Hz ... 16 kHz, +/-12 dB).
// Each band is a constant-Q RBJ peaking filter from JUCE's allocation-free
// ArrayCoefficients, so gain changes are safe on the audio thread.
class GraphicEQProcessor final
{
public:
    static constexpr int numBands = 10;
    static constexpr std::array<float, numBands> frequencies { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    static constexpr float maximumGainDb = 12.0f;

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = spec.sampleRate;
        for (auto& gain : gains)
            gain.prepare(sampleRate, 0.030, 0.0f);
        bypass.prepare(sampleRate, true);
        reset();
        updateCoefficients(true);
    }

    void reset() noexcept
    {
        for (auto& channel : state)
            for (auto& band : channel)
                band = {};
    }

    void setBandGainDb(int band, float db) noexcept
    {
        if (band >= 0 && band < numBands)
            gains[static_cast<size_t>(band)].set(juce::jlimit(-maximumGainDb, maximumGainDb, db));
    }
    void setBypassed(bool value) noexcept { bypass.setBypassed(value); }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels = std::min(2, buffer.getNumChannels());
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Coefficients follow the smoothed gains every 16 samples.
            if ((sample & 15) == 0)
                updateCoefficients(false);
            else
                for (auto& gain : gains)
                    gain.next();
            const auto wet = bypass.nextWetMix();
            if (wet <= 0.0f)
                continue;
            for (int channel = 0; channel < channels; ++channel)
            {
                auto* data = buffer.getWritePointer(channel);
                const auto dry = data[sample];
                auto value = dry;
                for (size_t band = 0; band < numBands; ++band)
                    if (active[band])
                        value = state[static_cast<size_t>(channel)][band].process(value, coefficients[band]);
                data[sample] = dry + wet * (value - dry);
            }
        }
    }

private:
    struct Section
    {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float process(float x, const std::array<float, 6>& c) noexcept
        {
            const auto y = c[0] * x + c[1] * x1 + c[2] * x2 - c[4] * y1 - c[5] * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    void updateCoefficients(bool force) noexcept
    {
        for (size_t band = 0; band < numBands; ++band)
        {
            const auto db = gains[band].next();
            if (! force && std::abs(db - currentDb[band]) < 1.0e-4f)
                continue;
            currentDb[band] = db;
            active[band] = std::abs(db) > 0.01f;
            const auto frequency = std::min(frequencies[band], static_cast<float>(sampleRate * 0.45));
            auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate, frequency, 1.41f, juce::Decibels::decibelsToGain(db));
            // Normalise by a0 so the section can run in direct form I.
            const auto a0 = raw[3];
            for (auto& value : raw)
                value /= a0;
            coefficients[band] = raw;
        }
    }

    double sampleRate = 48000.0;
    std::array<SmoothedParameter, numBands> gains;
    std::array<float, numBands> currentDb {};
    std::array<bool, numBands> active {};
    std::array<std::array<float, 6>, numBands> coefficients {};
    std::array<std::array<Section, numBands>, 2> state {};
    BypassRamp bypass;
};
} // namespace nam::effects
