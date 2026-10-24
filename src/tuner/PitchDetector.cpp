#include "PitchDetector.h"

#include <algorithm>

namespace tuner
{
PitchDetector::Result PitchDetector::detect(const float* samples, int count, double sampleRate, double lowest, double highest)
{
    const auto maxLag = std::min(count / 2, static_cast<int>(sampleRate / lowest));
    const auto minLag = std::max(2, static_cast<int>(sampleRate / highest));
    if (maxLag <= minLag + 2)
        return {};
    // Too quiet: no reading (the needle would follow noise).
    double energy = 0.0;
    for (int index = 0; index < count; ++index)
        energy += static_cast<double>(samples[index]) * samples[index];
    if (energy / count < 1.0e-7)
        return {};

    const auto window = count - maxLag;
    difference.assign(static_cast<size_t>(maxLag + 1), 0.0f);
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (int index = 0; index < window; ++index)
        {
            const auto delta = samples[index] - samples[index + lag];
            sum += static_cast<double>(delta) * delta;
        }
        difference[static_cast<size_t>(lag)] = static_cast<float>(sum);
    }
    // Cumulative mean normalised difference.
    difference[0] = 1.0f;
    double running = 0.0;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        running += difference[static_cast<size_t>(lag)];
        difference[static_cast<size_t>(lag)] = running > 0.0 ? static_cast<float>(difference[static_cast<size_t>(lag)] * lag / running) : 1.0f;
    }
    // The first dip below the threshold is the period (avoids octave errors).
    constexpr float threshold = 0.15f;
    int best = -1;
    for (int lag = minLag; lag < maxLag; ++lag)
        if (difference[static_cast<size_t>(lag)] < threshold)
        {
            while (lag + 1 < maxLag && difference[static_cast<size_t>(lag + 1)] < difference[static_cast<size_t>(lag)])
                ++lag;
            best = lag;
            break;
        }
    if (best < 0)
        return {};
    // Parabolic interpolation around the dip for sub-sample precision.
    const auto a = difference[static_cast<size_t>(best - 1)], b = difference[static_cast<size_t>(best)], c = difference[static_cast<size_t>(best + 1)];
    const auto denominator = a + c - 2.0f * b;
    const auto shift = std::abs(denominator) > 1.0e-9f ? 0.5f * (a - c) / denominator : 0.0f;
    const auto period = static_cast<double>(best) + juce::jlimit(-1.0f, 1.0f, shift);
    return { sampleRate / period, juce::jlimit(0.0, 1.0, 1.0 - static_cast<double>(b)) };
}

Note noteOf(double frequency, double referenceA4)
{
    static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
    Note note;
    if (frequency <= 0.0)
        return note;
    const auto exact = 69.0 + 12.0 * std::log2(frequency / referenceA4);
    note.midi = static_cast<int>(std::lround(exact));
    note.cents = (exact - note.midi) * 100.0;
    note.name = names[((note.midi % 12) + 12) % 12];
    note.octave = note.midi / 12 - 1;
    return note;
}
} // namespace tuner
