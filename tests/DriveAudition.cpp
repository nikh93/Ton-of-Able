// Standalone offline audition, deliberately independent of AudioEngine.
// Build with NAM_SAMPLE_FLOAT for this file AND all NAM translation units.
// Usage: DriveAudition input.wav output-directory model.nam cabinet.wav
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "audio/effects/DriveProcessor.h"
#include "NAM/get_dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<NAM_SAMPLE, float>, "Build all NAM sources with NAM_SAMPLE_FLOAT");

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 256;
constexpr float inputReference = 0.12f;
constexpr double peakLimit = 0.85;
using Buffer = juce::AudioBuffer<float>;

struct Measurements { double peak = 0.0, rms = 0.0; };

Measurements measure(const Buffer& audio)
{
    Measurements result;
    double sum = 0.0;
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const double x = audio.getSample(0, i);
        if (!std::isfinite(x))
            throw std::runtime_error("Non-finite audio sample");
        result.peak = std::max(result.peak, std::abs(x));
        sum += x * x;
    }
    result.rms = std::sqrt(sum / std::max(1, audio.getNumSamples()));
    return result;
}

double db(double value) { return value > 0.0 ? 20.0 * std::log10(value) : -std::numeric_limits<double>::infinity(); }

std::unique_ptr<juce::AudioFormatReader> openWav(const juce::File& file)
{
    juce::WavAudioFormat format;
    auto stream = file.createInputStream();
    if (!stream)
        throw std::runtime_error("Cannot open WAV: " + file.getFullPathName().toStdString());
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    if (!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate <= 0.0
        || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        throw std::runtime_error("Invalid or empty WAV: " + file.getFullPathName().toStdString());
    return reader;
}

// Average all source channels once. Every variant subsequently uses this same DI.
Buffer readMono(juce::AudioFormatReader& reader, int frames)
{
    Buffer multichannel(static_cast<int>(reader.numChannels), frames);
    if (!reader.read(&multichannel, 0, frames, 0, true, true))
        throw std::runtime_error("WAV decode failed");
    Buffer mono(1, frames);
    mono.clear();
    for (int ch = 0; ch < multichannel.getNumChannels(); ++ch)
        mono.addFrom(0, 0, multichannel, ch, 0, frames, 1.0f / static_cast<float>(reader.numChannels));
    measure(mono);
    return mono;
}

class MemorySource final : public juce::AudioSource
{
public:
    explicit MemorySource(const Buffer& b) : audio(b) {}
    void prepareToPlay(int, double) override { position = 0; }
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        const int count = std::min(info.numSamples, audio.getNumSamples() - position);
        if (count > 0)
        {
            info.buffer->copyFrom(0, info.startSample, audio, 0, position, count);
            position += count;
        }
    }
private:
    const Buffer& audio;
    int position = 0;
};

Buffer prepareInput(juce::AudioFormatReader& reader)
{
    const auto frames = static_cast<int>(std::min<juce::int64>(reader.lengthInSamples,
                                            static_cast<juce::int64>(12.0 * reader.sampleRate)));
    if (frames <= 0)
        throw std::runtime_error("Input is too short");
    auto mono = readMono(reader, frames);
    Buffer input(1, std::min(576000, static_cast<int>(std::floor(frames * rate / reader.sampleRate))));
    if (input.getNumSamples() == 0)
        throw std::runtime_error("Input is too short at 48 kHz");
    if (reader.sampleRate == rate)
        input.copyFrom(0, 0, mono, 0, 0, input.getNumSamples());
    else
    {
        MemorySource source(mono);
        juce::ResamplingAudioSource resampler(&source, false, 1);
        resampler.setResamplingRatio(reader.sampleRate / rate);
        resampler.prepareToPlay(blockSize, rate);
        for (int offset = 0; offset < input.getNumSamples(); offset += blockSize)
            resampler.getNextAudioBlock({ &input, offset, std::min(blockSize, input.getNumSamples() - offset) });
        resampler.releaseResources();
    }
    const auto before = measure(input);
    if (before.peak <= 0.0)
        throw std::runtime_error("Silent mono input cannot be normalised");
    const double gain = inputReference / before.peak;
    // One constant gain, no compression, gating, or per-block normalisation.
    for (int i = 0; i < input.getNumSamples(); ++i)
        input.setSample(0, i, static_cast<float>(input.getSample(0, i) * gain));
    const auto after = measure(input);
    std::cout << "Input: source=" << reader.sampleRate << " Hz, channels=" << reader.numChannels
              << " (mono average), rendered=" << input.getNumSamples() / rate << " s at 48000 Hz\n"
              << "Digital test reference only (no dBu calibration): peak=" << after.peak
              << " (" << db(after.peak) << " dBFS), RMS=" << after.rms
              << ", constant gain=" << db(gain) << " dB, pre-reference peak=" << before.peak << '\n';
    return input;
}

struct Variant
{
    const char* name;
    nam::effects::DriveProcessor::DriveType type;
    bool bypass;
    float drive, tone, warmth, presence, sustain;
};

Buffer render(const Buffer& input, const Buffer& ir, double irRate,
              const juce::File& modelFile, const Variant& variant)
{
    const juce::dsp::ProcessSpec spec { rate, blockSize, 1 };
    nam::DspLoadOptions options;
    options.prewarm = false;
    auto model = nam::get_dsp(std::filesystem::path(modelFile.getFullPathName().toWideCharPointer()), options);
    if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
        throw std::runtime_error("Audition requires a mono NAM model");
    const double expected = model->GetExpectedSampleRate();
    if (expected > 0.0 && std::abs(expected - rate) > 0.5)
        throw std::runtime_error("NAM declares a sample rate other than 48000 Hz");
    model->SetPrewarmOnReset(false);
    model->Reset(rate, blockSize);
    model->prewarm();

    nam::effects::DriveProcessor drive;
    drive.prepare(spec);
    drive.setType(variant.type);
    drive.setDrive(variant.drive);
    drive.setTone(variant.tone);
    drive.setWarmth(variant.warmth);
    drive.setPresence(variant.presence);
    drive.setSustain(variant.sustain);
    drive.setBias(.35f);
    drive.setLevel(1.0f);
    drive.setMix(1.0f);
    drive.setBypassed(variant.bypass);
    // Set after prepare (which may initialise controls), then settle smoothing
    // and bypass ramps before the DI starts. Do not reset controls afterwards.
    Buffer settling(1, blockSize);
    for (int i = 0; i < 188; ++i)
    {
        settling.clear();
        drive.process(settling);
    }

    juce::dsp::Convolution cabinet;
    Buffer irCopy;
    irCopy.makeCopyOf(ir);
    cabinet.loadImpulseResponse(std::move(irCopy), irRate, juce::dsp::Convolution::Stereo::no,
                                juce::dsp::Convolution::Trim::yes,
                                juce::dsp::Convolution::Normalise::yes);
    // JUCE guarantees the last loaded IR is active on the FIRST process after prepare.
    cabinet.prepare(spec);
    if (cabinet.getCurrentIRSize() <= 0)
        throw std::runtime_error("Cabinet IR did not initialise");

    // Identical measurement windows, including an IR-length tail plus one second
    // for the pedal/model decay. No latency trimming or alignment modifications.
    const int tail = static_cast<int>(std::ceil(ir.getNumSamples() * rate / irRate)) + 48000;
    Buffer output(1, input.getNumSamples() + tail);
    for (int offset = 0; offset < output.getNumSamples(); offset += blockSize)
    {
        const int count = std::min(blockSize, output.getNumSamples() - offset);
        Buffer block(1, count); // Exact length, including the final partial block.
        block.clear();
        const int available = std::min(count, input.getNumSamples() - offset);
        if (available > 0)
            block.copyFrom(0, 0, input, 0, offset, available);
        drive.process(block);
        NAM_SAMPLE* channels[] { block.getWritePointer(0) };
        model->process(channels, channels, count);
        auto audioBlock = juce::dsp::AudioBlock<float>(block);
        cabinet.process(juce::dsp::ProcessContextReplacing<float>(audioBlock));
        output.copyFrom(0, offset, block, 0, 0, count);
    }
    std::cout << variant.name << ": Drive=" << variant.drive << ", tone=" << variant.tone
              << ", warmth=" << variant.warmth << ", presence=" << variant.presence
              << ", sustain=" << variant.sustain << ", bias=.35, level=1, mix=1, bypass=" << variant.bypass
              << "; NAM rate=" << (expected > 0 ? std::to_string(expected) : "unknown (test assumes 48000)")
              << ", drive latency=" << drive.latencySamples() << ", cabinet latency=" << cabinet.getLatency()
              << " samples\n";
    return output;
}

void writeWav(const juce::File& file, const Buffer& audio)
{
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (!stream)
        throw std::runtime_error("Cannot create output WAV");
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                        .withSampleRate(rate).withNumChannels(1).withBitsPerSample(24));
    if (!writer || !writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()) || !writer->flush())
        throw std::runtime_error("Failed writing WAV: " + file.getFullPathName().toStdString());
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 5)
        {
            std::cerr << "Usage: DriveAudition input.wav output-directory model.nam cabinet.wav\n";
            return 2;
        }
        juce::ScopedNoDenormals noDenormals;
        std::cout << std::fixed << std::setprecision(6);
        const auto cwd = juce::File::getCurrentWorkingDirectory();
        const auto inputFile = cwd.getChildFile(juce::String::fromUTF8(argv[1]));
        const auto directory = cwd.getChildFile(juce::String::fromUTF8(argv[2]));
        const auto modelFile = cwd.getChildFile(juce::String::fromUTF8(argv[3]));
        const auto irFile = cwd.getChildFile(juce::String::fromUTF8(argv[4]));
        using Type = nam::effects::DriveProcessor::DriveType;
        const std::array<Variant, 3> variants {{
            { "dry_drive_bypassed", Type::WarmOverdrive, true, .48f, .45f, .7f, .5f, .5f },
            { "warm_overdrive", Type::WarmOverdrive, false, .48f, .45f, .7f, .5f, .5f },
            { "singing_lead", Type::SingingLead, false, .52f, .38f, .5f, .4f, .65f }
        }};
        for (const auto& v : variants)
            if (directory.getChildFile(juce::String(v.name) + ".wav").exists())
                throw std::runtime_error("Output already exists; choose a fresh output directory");
        auto inputReader = openWav(inputFile);
        auto input = prepareInput(*inputReader);
        auto irReader = openWav(irFile);
        if (irReader->lengthInSamples > 30.0 * irReader->sampleRate
            || irReader->lengthInSamples > std::numeric_limits<int>::max())
            throw std::runtime_error("Cabinet IR exceeds 30 seconds");
        auto ir = readMono(*irReader, static_cast<int>(irReader->lengthInSamples));
        if (measure(ir).peak == 0.0)
            throw std::runtime_error("Cabinet IR is silent after mono averaging");
        std::cout << "NAM: " << modelFile.getFullPathName() << "\nCabinet: " << irFile.getFullPathName()
                  << " (mono average, trim=yes, normalise=yes)\n";
        std::vector<Buffer> outputs;
        std::vector<Measurements> raw;
        double commonRms = 0.1; // Aim at -20 dBFS RMS, reduced for ALL files if any peak requires it.
        for (const auto& v : variants)
        {
            outputs.push_back(render(input, ir, irReader->sampleRate, modelFile, v));
            raw.push_back(measure(outputs.back()));
            if (raw.back().rms <= 0.0)
                throw std::runtime_error("Silent render cannot be RMS matched");
            // Small quantisation/rounding margin below the requested 0.85 ceiling.
            commonRms = std::min(commonRms, (peakLimit - 1.0e-6) * raw.back().rms / raw.back().peak);
        }
        if (directory.createDirectory().failed())
            throw std::runtime_error("Cannot create output directory");
        std::cout << "Common RMS=" << commonRms << " (" << db(commonRms)
                  << " dBFS); measured over full equal-length files including tails; constant gain only.\n";
        for (size_t i = 0; i < outputs.size(); ++i)
        {
            const double gain = commonRms / raw[i].rms;
            for (int s = 0; s < outputs[i].getNumSamples(); ++s)
                outputs[i].setSample(0, s, static_cast<float>(outputs[i].getSample(0, s) * gain));
            const auto matched = measure(outputs[i]);
            if (matched.peak > peakLimit)
                throw std::runtime_error("Peak constraint failed");
            const auto file = directory.getChildFile(juce::String(variants[i].name) + ".wav");
            writeWav(file, outputs[i]);
            auto checkReader = openWav(file);
            const auto written = measure(readMono(*checkReader, static_cast<int>(checkReader->lengthInSamples)));
            if (checkReader->bitsPerSample != 24 || written.peak > peakLimit
                || std::abs(written.rms - commonRms) > 2.0e-7)
                throw std::runtime_error("Written WAV verification failed");
            std::cout << variants[i].name << ": raw peak=" << raw[i].peak << ", raw RMS=" << raw[i].rms
                      << ", gain=" << db(gain) << " dB; WAV peak=" << written.peak << ", RMS=" << written.rms
                      << ", seconds=" << outputs[i].getNumSamples() / rate << ", 48000 Hz mono PCM24\n";
        }
        std::cout << "Rendered and measured offline; no listening assessment performed.\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "DriveAudition: " << e.what() << '\n';
        return 1;
    }
}
