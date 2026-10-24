#pragma once

#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

namespace tuner
{
/** Monophonic pitch detection (YIN) for tuning instruments: low B of a
    bass (31 Hz) up to the highest guitar frets (~1.5 kHz). Not real-time
    safe (allocates on the first call); run it off the audio thread. */
class PitchDetector
{
public:
    struct Result
    {
        double frequency = 0.0; // 0: nothing clear (silence, noise, chords)
        double clarity = 0.0;   // 0 ... 1
    };

    /** At least two periods of the lowest note are needed: 4096 samples at 48 kHz cover 25 Hz. */
    [[nodiscard]] Result detect(const float* samples, int count, double sampleRate, double lowest = 28.0, double highest = 1600.0);

private:
    std::vector<float> difference;
};

struct Note
{
    int midi = 0;          // nearest MIDI note
    double cents = 0.0;    // -50 ... +50 from it
    juce::String name;     // "E", "A#" ...
    int octave = 0;
};

/** The nearest note to a frequency for a reference pitch (A4 = 440 Hz by default). */
[[nodiscard]] Note noteOf(double frequency, double referenceA4 = 440.0);
} // namespace tuner
