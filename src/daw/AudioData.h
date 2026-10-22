#pragma once

#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

namespace daw
{
// Immutable, fully decoded audio in the project's sample rate. Instances are
// shared between clips, undo states, the realtime snapshot and offline
// renders; they are never modified once published.
struct AudioData
{
    static constexpr int peakBlock = 256;

    juce::AudioBuffer<float> samples; // one or two channels
    double sampleRate = 0.0;
    std::vector<float> peaks;         // max |x| of all channels per peakBlock samples

    [[nodiscard]] juce::int64 length() const noexcept { return samples.getNumSamples(); }
    [[nodiscard]] int numChannels() const noexcept { return samples.getNumChannels(); }
    void computePeaks();
    /** Largest peak in [from, to) source samples; used for waveform drawing. */
    [[nodiscard]] float peakBetween(double from, double to) const noexcept;
};

using AudioDataPtr = std::shared_ptr<const AudioData>;

/** Decodes an audio file on a worker thread and converts it to targetRate.
    Mono stays mono; files with more than two channels keep the first two. */
[[nodiscard]] std::shared_ptr<AudioData> loadAudioFile(const juce::File& file, double targetRate, juce::String& error);

/** Band-limited offline sample-rate conversion (windowed sinc). */
[[nodiscard]] juce::AudioBuffer<float> resample(const juce::AudioBuffer<float>& input, double inputRate, double outputRate);

/** File extensions accepted by loadAudioFile, e.g. "*.wav;*.aif;...". */
[[nodiscard]] juce::String supportedAudioWildcard();
} // namespace daw
