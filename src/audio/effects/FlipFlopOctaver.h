#pragma once

#include <array>
#include <cmath>

#include <juce_core/juce_core.h>

namespace nam::effects
{
// Analog-style octave down as in classic octave pedals: the signal is
// low-passed to its fundamental, a Schmitt trigger turns it into a square
// wave and a flip-flop divides that by two. The divided square follows the
// playing level and is smoothed by a tone filter - one octave lower.
//
// Works sample by sample without any buffering, so there is no latency apart
// from the phase shift of the filters (one to two milliseconds), just like
// the pedals. Like them it is monophonic and only does exactly -12.
//
// A single fixed tracking filter cannot cover a guitar's range: tuned low
// it buries high notes under rumble, tuned high it lets the second harmonic
// of low notes through and the flip-flop toggles twice per period. So a small
// bank of octave-spaced low-passes runs in parallel and the comparator listens
// to the lowest one that still carries real signal - a note's fundamental is
// its lowest component, and in that band it dominates its overtones.
class FlipFlopOctaver final
{
public:
    void prepare(double sampleRate) noexcept
    {
        rate = sampleRate;
        highPassCoefficient = coefficient(40.0);
        attackCoefficient = coefficient(2000.0);
        releaseCoefficient = coefficient(8.0);
        gateCoefficient = coefficient(60.0);
        for (size_t index = 0; index < numBands; ++index)
        {
            bandCoefficient[index] = coefficient(bandCutoff[index]);
            toneForBand[index] = coefficient(bandCutoff[index] * 1.5);
        }
        switchHold = static_cast<int>(sampleRate * 0.005);
        reset();
    }

    void reset() noexcept
    {
        highPass = {};
        for (auto& stages : bandState)
            stages = {};
        bandEnvelope = {};
        inputEnvelope = gate = 0.0f;
        tone = {};
        band = candidate = numBands - 1;
        candidateCount = 0;
        comparatorHigh = false;
        divider = 1.0f;
    }

    float process(float input) noexcept
    {
        auto x = input;
        for (auto& state : highPass) // removes rumble and DC
        {
            state += highPassCoefficient * (x - state);
            x -= state;
        }
        follow(inputEnvelope, std::abs(x));

        std::array<float, numBands> bandOutput {};
        for (size_t index = 0; index < numBands; ++index)
        {
            auto y = x;
            for (auto& stage : bandState[index])
                y = stage += bandCoefficient[index] * (y - stage);
            bandOutput[index] = y;
            follow(bandEnvelope[index], std::abs(y));
        }

        // Lowest band carrying signal; switch only once the choice is stable.
        auto wanted = numBands - 1;
        for (size_t index = 0; index < numBands; ++index)
            if (bandEnvelope[index] > bandThreshold * inputEnvelope)
            {
                wanted = index;
                break;
            }
        if (wanted == band)
            candidateCount = 0;
        else
        {
            candidateCount = wanted == candidate ? candidateCount + 1 : 1;
            candidate = wanted;
            if (candidateCount >= switchHold)
            {
                band = wanted;
                candidateCount = 0;
            }
        }

        // Schmitt trigger with hysteresis relative to the band level, then the flip-flop.
        const auto tracked = bandOutput[band];
        const auto threshold = hysteresis * bandEnvelope[band];
        if (! comparatorHigh && tracked > threshold)
        {
            comparatorHigh = true;
            divider = -divider;
        }
        else if (comparatorHigh && tracked < -threshold)
            comparatorHigh = false;

        // Fade out below the noise floor instead of toggling on hiss.
        gate += gateCoefficient * ((inputEnvelope > gateLevel ? 1.0f : 0.0f) - gate);

        auto y = divider * inputEnvelope * gate;
        for (auto& stage : tone)
            y = stage += toneForBand[band] * (y - stage);
        return y;
    }

private:
    static constexpr size_t numBands = 5;
    static constexpr std::array<double, numBands> bandCutoff { 70.0, 140.0, 280.0, 560.0, 1120.0 };
    static constexpr float bandThreshold = 0.016f; // -36 dB below the input level
    static constexpr float hysteresis = 0.45f;
    static constexpr float gateLevel = 3.0e-4f;    // about -70 dBFS

    [[nodiscard]] float coefficient(double hz) const noexcept
    {
        return static_cast<float>(1.0 - std::exp(-juce::MathConstants<double>::twoPi * hz / rate));
    }

    void follow(float& envelope, float level) const noexcept
    {
        envelope += (level > envelope ? attackCoefficient : releaseCoefficient) * (level - envelope);
    }

    double rate = 48000.0;
    float highPassCoefficient = 0.0f, attackCoefficient = 0.0f, releaseCoefficient = 0.0f, gateCoefficient = 0.0f;
    std::array<float, numBands> bandCoefficient {}, toneForBand {};
    std::array<float, 2> highPass {};
    std::array<std::array<float, 4>, numBands> bandState {};
    std::array<float, numBands> bandEnvelope {};
    std::array<float, 2> tone {};
    float inputEnvelope = 0.0f, gate = 0.0f;
    size_t band = numBands - 1, candidate = numBands - 1;
    int candidateCount = 0, switchHold = 240;
    bool comparatorHigh = false;
    float divider = 1.0f;
};
} // namespace nam::effects
