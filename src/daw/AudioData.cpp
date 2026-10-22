#include "AudioData.h"
#include "DawText.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace daw
{
void AudioData::computePeaks()
{
    const auto total = samples.getNumSamples();
    peaks.assign(static_cast<size_t>((total + peakBlock - 1) / peakBlock), 0.0f);
    for (int channel = 0; channel < samples.getNumChannels(); ++channel)
    {
        const auto* data = samples.getReadPointer(channel);
        for (size_t block = 0; block < peaks.size(); ++block)
        {
            const auto begin = static_cast<int>(block) * peakBlock;
            const auto count = std::min(peakBlock, total - begin);
            const auto range = juce::FloatVectorOperations::findMinAndMax(data + begin, count);
            peaks[block] = std::max(peaks[block], std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
        }
    }
}

float AudioData::peakBetween(double from, double to) const noexcept
{
    if (peaks.empty())
        return 0.0f;
    const auto last = static_cast<juce::int64>(peaks.size()) - 1;
    auto first = juce::jlimit<juce::int64>(0, last, static_cast<juce::int64>(std::floor(from / peakBlock)));
    const auto final = juce::jlimit<juce::int64>(first, last, static_cast<juce::int64>(std::floor(std::max(from, to - 1.0) / peakBlock)));
    float peak = 0.0f;
    for (; first <= final; ++first)
        peak = std::max(peak, peaks[static_cast<size_t>(first)]);
    return peak;
}

juce::AudioBuffer<float> resample(const juce::AudioBuffer<float>& input, const double inputRate, const double outputRate)
{
    const auto inputLength = input.getNumSamples();
    if (inputRate <= 0.0 || outputRate <= 0.0 || std::abs(inputRate - outputRate) < 1.0e-9)
        return input;

    const auto outputLength = static_cast<int>(std::ceil(static_cast<double>(inputLength) * outputRate / inputRate));
    juce::AudioBuffer<float> output(input.getNumChannels(), std::max(0, outputLength));

    // Same 64-tap Blackman windowed-sinc kernel as FilePlayer: slow but clean,
    // and only ever executed on worker threads.
    constexpr int halfTaps = 32;
    constexpr double pi = 3.1415926535897932384626433832795;
    const auto ratio = inputRate / outputRate;
    const auto cutoff = 0.5 * std::min(1.0, outputRate / inputRate);
    std::vector<double> coefficients(2 * halfTaps);
    for (int outputIndex = 0; outputIndex < output.getNumSamples(); ++outputIndex)
    {
        const auto sourcePosition = static_cast<double>(outputIndex) * ratio;
        const auto centre = static_cast<int>(std::floor(sourcePosition));
        double normaliser = 0.0;
        for (int tap = -halfTaps + 1; tap <= halfTaps; ++tap)
        {
            const auto x = sourcePosition - static_cast<double>(centre + tap);
            const auto sinc = std::abs(x) < 1.0e-12 ? 1.0 : std::sin(2.0 * pi * cutoff * x) / (pi * x);
            const auto phase = (static_cast<double>(tap) + halfTaps - 1.0) / (2.0 * halfTaps - 1.0);
            const auto window = 0.42 - 0.5 * std::cos(2.0 * pi * phase) + 0.08 * std::cos(4.0 * pi * phase);
            const auto index = centre + tap;
            coefficients[static_cast<size_t>(tap + halfTaps - 1)] = index >= 0 && index < inputLength ? sinc * window : 0.0;
            normaliser += coefficients[static_cast<size_t>(tap + halfTaps - 1)];
        }
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
        {
            const auto* source = input.getReadPointer(channel);
            double sum = 0.0;
            for (int tap = -halfTaps + 1; tap <= halfTaps; ++tap)
                if (const auto index = centre + tap; index >= 0 && index < inputLength)
                    sum += static_cast<double>(source[index]) * coefficients[static_cast<size_t>(tap + halfTaps - 1)];
            output.setSample(channel, outputIndex, static_cast<float>(normaliser != 0.0 ? sum / normaliser : 0.0));
        }
    }
    return output;
}

std::shared_ptr<AudioData> loadAudioFile(const juce::File& file, const double targetRate, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "Audiodatei fehlt: " + file.getFullPathName();
        return {};
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
    {
        error = text("Format wird nicht unterstützt: ") + file.getFileName();
        return {};
    }
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max() / 2)
    {
        error = text("Ungültige Länge: ") + file.getFileName();
        return {};
    }

    const auto channels = static_cast<int>(std::min(2u, reader->numChannels));
    juce::AudioBuffer<float> decoded(channels, static_cast<int>(reader->lengthInSamples));
    if (! reader->read(&decoded, 0, decoded.getNumSamples(), 0, true, channels > 1))
    {
        error = "Datei konnte nicht dekodiert werden: " + file.getFileName();
        return {};
    }

    auto data = std::make_shared<AudioData>();
    const auto rate = targetRate > 0.0 ? targetRate : reader->sampleRate;
    data->samples = resample(decoded, reader->sampleRate, rate);
    data->sampleRate = rate;
    data->computePeaks();
    return data;
}

juce::String supportedAudioWildcard()
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    return formats.getWildcardForAllFormats();
}
} // namespace daw
