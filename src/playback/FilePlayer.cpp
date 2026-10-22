#include "FilePlayer.h"

#include <algorithm>
#include <cmath>
#include <limits>

FilePlayer::FilePlayer()
{
    formats.registerBasicFormats();
}

juce::Result FilePlayer::load(const juce::File& file)
{
    if (! file.existsAsFile())
        return juce::Result::fail("WAV file does not exist: " + file.getFullPathName());

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
        return juce::Result::fail("The file is not a supported WAV audio file.");
    if (reader->numChannels < 1 || reader->numChannels > 2)
        return juce::Result::fail("Please use a mono or stereo WAV file (1 or 2 channels).");
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max())
        return juce::Result::fail("The WAV file has an unsupported length.");

    auto decoded = std::make_unique<DecodedSource>();
    decoded->samples.setSize(1, static_cast<int>(reader->lengthInSamples), false, true, false);
    // Decode and fold down off the audio thread. Averaging L/R preserves the
    // level of dual-mono material and avoids the +6 dB of an unscaled sum.
    // Use bounded scratch storage even for long stereo recordings.
    juce::AudioBuffer<float> block(static_cast<int>(reader->numChannels), 8192);
    for (int offset = 0; offset < decoded->samples.getNumSamples();)
    {
        const auto count = std::min(block.getNumSamples(), decoded->samples.getNumSamples() - offset);
        if (! reader->read(&block, 0, count, offset, true, reader->numChannels == 2))
            return juce::Result::fail("Could not decode the WAV file.");
        decoded->samples.copyFrom(0, offset, block, 0, 0, count);
        if (reader->numChannels == 2)
        {
            decoded->samples.applyGain(0, offset, count, 0.5f);
            decoded->samples.addFrom(0, offset, block, 1, 0, count, 0.5f);
        }
        offset += count;
    }

    decoded->sampleRate = reader->sampleRate;
    decoded->fileName = file.getFileName();
    latestDecodedSource = decoded.get();
    retainedDecodedSources.push_back(std::move(decoded));
    return publishRenderedSource(*latestDecodedSource);
}

juce::Result FilePlayer::setOutputSampleRate(const double sampleRate)
{
    if (! std::isfinite(sampleRate) || sampleRate < 0.0)
        return juce::Result::fail("Output sample rate must be zero or a positive finite value.");

    outputSampleRate = sampleRate;
    return latestDecodedSource != nullptr ? publishRenderedSource(*latestDecodedSource) : juce::Result::ok();
}

std::unique_ptr<FilePlayer::Source> FilePlayer::renderForOutputRate(const DecodedSource& decoded,
                                                                      const double requestedOutputRate)
{
    const auto inputRate = decoded.sampleRate;
    const auto outputRate = requestedOutputRate > 0.0 ? requestedOutputRate : inputRate;
    const auto inputLength = decoded.samples.getNumSamples();
    if (inputRate <= 0.0 || outputRate <= 0.0 || inputLength <= 0)
        return {};

    const auto outputLength64 = static_cast<juce::int64>(std::ceil(static_cast<double>(inputLength) * outputRate / inputRate));
    if (outputLength64 <= 0 || outputLength64 > std::numeric_limits<int>::max())
        return {};

    auto rendered = std::make_unique<Source>();
    rendered->samples.setSize(1, static_cast<int>(outputLength64), false, true, false);
    rendered->sampleRate = outputRate;
    rendered->fileName = decoded.fileName;

    const auto* input = decoded.samples.getReadPointer(0);
    auto* output = rendered->samples.getWritePointer(0);
    if (std::abs(inputRate - outputRate) < 1.0e-9)
    {
        juce::FloatVectorOperations::copy(output, input, inputLength);
        return rendered;
    }

    // Offline, band-limited windowed-sinc resampling. The 64-tap Blackman
    // kernel is deliberately built here rather than in process(), preserving
    // the callback's hard real-time constraints and significantly reducing
    // aliasing compared with linear interpolation when downsampling.
    constexpr int halfTaps = 32;
    constexpr double pi = 3.1415926535897932384626433832795;
    const auto ratio = inputRate / outputRate;
    const auto cutoff = 0.5 * std::min(1.0, outputRate / inputRate);
    for (int outputIndex = 0; outputIndex < static_cast<int>(outputLength64); ++outputIndex)
    {
        const auto sourcePosition = static_cast<double>(outputIndex) * ratio;
        const auto centre = static_cast<int>(std::floor(sourcePosition));
        double sum = 0.0;
        double normaliser = 0.0;
        for (int tap = -halfTaps + 1; tap <= halfTaps; ++tap)
        {
            const auto sourceIndex = centre + tap;
            if (sourceIndex < 0 || sourceIndex >= inputLength)
                continue;

            const auto x = sourcePosition - static_cast<double>(sourceIndex);
            const auto sinc = std::abs(x) < 1.0e-12 ? 1.0 : std::sin(2.0 * pi * cutoff * x) / (pi * x);
            const auto phase = (static_cast<double>(tap) + halfTaps - 1.0) / (2.0 * halfTaps - 1.0);
            const auto window = 0.42 - 0.5 * std::cos(2.0 * pi * phase) + 0.08 * std::cos(4.0 * pi * phase);
            const auto coefficient = sinc * window;
            sum += static_cast<double>(input[sourceIndex]) * coefficient;
            normaliser += coefficient;
        }
        output[outputIndex] = static_cast<float>(normaliser != 0.0 ? sum / normaliser : 0.0);
    }
    return rendered;
}

juce::Result FilePlayer::publishRenderedSource(const DecodedSource& decoded)
{
    auto source = renderForOutputRate(decoded, outputSampleRate);
    if (source == nullptr)
        return juce::Result::fail("Could not resample the WAV file to the requested output sample rate.");

    auto* published = source.get();
    retainedSources.push_back(std::move(source));
    position.store(0, std::memory_order_release);
    current.store(published, std::memory_order_release);
    state.store(State::stopped, std::memory_order_release);
    return juce::Result::ok();
}

void FilePlayer::play() noexcept
{
    if (current.load(std::memory_order_acquire) != nullptr)
        state.store(State::playing, std::memory_order_release);
}

void FilePlayer::pause() noexcept { state.store(State::paused, std::memory_order_release); }
void FilePlayer::stop() noexcept { position.store(0, std::memory_order_release); state.store(State::stopped, std::memory_order_release); }
void FilePlayer::setLooping(const bool shouldLoop) noexcept { looping.store(shouldLoop, std::memory_order_release); }
bool FilePlayer::isLooping() const noexcept { return looping.load(std::memory_order_acquire); }

void FilePlayer::setPositionNormalized(const float normalized) noexcept
{
    const auto* source = current.load(std::memory_order_acquire);
    if (source == nullptr)
        return;
    const auto length = source->samples.getNumSamples();
    const auto clamped = juce::jlimit(0.0f, 1.0f, normalized);
    position.store(static_cast<juce::int64>(clamped * static_cast<float>(juce::jmax(0, length - 1))),
                  std::memory_order_release);
}

float FilePlayer::positionNormalized() const noexcept
{
    const auto* source = current.load(std::memory_order_acquire);
    if (source == nullptr || source->samples.getNumSamples() <= 1)
        return 0.0f;
    return juce::jlimit(0.0f, 1.0f,
                        static_cast<float>(position.load(std::memory_order_acquire))
                            / static_cast<float>(source->samples.getNumSamples() - 1));
}

void FilePlayer::process(float* destination, const int numSamples) noexcept
{
    if (destination == nullptr || numSamples <= 0)
        return;

    juce::FloatVectorOperations::clear(destination, numSamples);
    auto* source = current.load(std::memory_order_acquire);
    if (source == nullptr || state.load(std::memory_order_acquire) != State::playing)
        return;

    const auto length = static_cast<juce::int64>(source->samples.getNumSamples());
    auto cursor = position.load(std::memory_order_relaxed);
    const bool shouldLoop = looping.load(std::memory_order_relaxed);
    int written = 0;
    while (written < numSamples && cursor < length)
    {
        const auto available = static_cast<int>(std::min<juce::int64>(length - cursor, numSamples - written));
        juce::FloatVectorOperations::copy(destination + written, source->samples.getReadPointer(0, static_cast<int>(cursor)), available);
        written += available;
        cursor += available;
        if (cursor == length && shouldLoop)
            cursor = 0;
    }

    if (cursor == length && ! shouldLoop)
        state.store(State::stopped, std::memory_order_release);
    position.store(cursor, std::memory_order_release);
}

FilePlayer::Status FilePlayer::getStatus() const
{
    const auto* source = current.load(std::memory_order_acquire);
    return { state.load(std::memory_order_acquire), looping.load(std::memory_order_acquire), source != nullptr,
             position.load(std::memory_order_acquire), source != nullptr ? source->samples.getNumSamples() : 0,
             source != nullptr ? source->sampleRate : 0.0, source != nullptr ? source->fileName : juce::String() };
}
