#include <cmath>

#include <juce_core/juce_core.h>

#include "tuner/PitchDetector.h"

namespace
{
// A plucked-string-like tone: fundamental plus decaying harmonics.
std::vector<float> tone(double frequency, double rate, int count, double detuneCents = 0.0)
{
    std::vector<float> out(static_cast<size_t>(count));
    const auto f = frequency * std::pow(2.0, detuneCents / 1200.0);
    for (int index = 0; index < count; ++index)
    {
        const auto t = index / rate;
        double value = 0.0;
        for (int harmonic = 1; harmonic <= 6; ++harmonic)
            value += std::sin(2.0 * juce::MathConstants<double>::pi * f * harmonic * t) / (harmonic * harmonic * 0.7);
        out[static_cast<size_t>(index)] = static_cast<float>(0.3 * value);
    }
    return out;
}

class TunerTests final : public juce::UnitTest
{
public:
    TunerTests() : UnitTest("Tuner", "Tuner") {}

    void runTest() override
    {
        tuner::PitchDetector detector;
        beginTest("Guitar and bass strings are found to a cent");
        for (const auto frequency : { 41.20, 61.74, 82.41, 110.0, 146.83, 196.0, 246.94, 329.63, 659.26, 1046.5 })
        {
            const auto signal = tone(frequency, 48000.0, 4096);
            const auto result = detector.detect(signal.data(), 4096, 48000.0);
            const auto cents = 1200.0 * std::log2(result.frequency / frequency);
            expect(std::abs(cents) < 1.0, juce::String(frequency) + " Hz -> " + juce::String(result.frequency, 3));
            expect(result.clarity > 0.8);
        }

        beginTest("Detuning is reported in cents, notes use German names");
        {
            const auto signal = tone(110.0, 44100.0, 4096, -12.0);
            const auto note = tuner::noteOf(detector.detect(signal.data(), 4096, 44100.0).frequency);
            expectEquals(note.name, juce::String("A"));
            expectEquals(note.octave, 2);
            expectWithinAbsoluteError(note.cents, -12.0, 1.0);
            expectEquals(tuner::noteOf(246.94).name, juce::String("H"));
            expectWithinAbsoluteError(tuner::noteOf(442.0, 442.0).cents, 0.0, 1.0e-9);
        }

        beginTest("Silence gives no reading");
        {
            std::vector<float> silence(4096, 0.0f);
            expectEquals(detector.detect(silence.data(), 4096, 48000.0).frequency, 0.0);
        }
    }
};

TunerTests tunerTests;
} // namespace
