#include "LivePitchShifter.h"

#include <algorithm>
#include <cmath>

#include <rubberband/RubberBandLiveShifter.h>
#include <rubberband/RubberBandStretcher.h>

namespace nam::effects
{
namespace
{
constexpr size_t ringCapacity = 1 << 15;

// Output cushion of the low-latency engine. R2 delivers audio in uneven hops,
// so playback of the queue starts only once this much is buffered beyond one
// block. Values are the largest shortfall measured at 48 kHz for each
// semitone (-24 ... +24) over device blocks of 32 ... 1024 samples, plus 15 %.
int lowLatencyReserve(double sampleRate, double semitones)
{
    static constexpr int measuredAt48k[] { 292, 330, 462, 179, 219, 389, 431, 607, 87, 209, 235, 363, 395, 425, 556, 433, 473, 644, 560, 546, 335, 387, 636, 695, 256, 894, 872, 840, 817, 849, 832, 807, 786, 764, 615, 693, 683, 879, 798, 829, 835, 876, 869, 885, 898, 884, 891, 843, 847 };
    const auto index = juce::jlimit(0, 48, static_cast<int>(std::lround(semitones)) + 24);
    return static_cast<int>(std::lround((measuredAt48k[index] * 1.15 + 32.0) * sampleRate / 48000.0));
}
} // namespace

LivePitchShifter::LivePitchShifter() = default;
LivePitchShifter::~LivePitchShifter() = default;

void LivePitchShifter::prepare(double sampleRate, int maximumBlockSize)
{
    using RubberBand::RubberBandStretcher;
    using RubberBand::RubberBandLiveShifter;
    rate = sampleRate;
    maximumBlock = std::max(1, maximumBlockSize);
    const auto sampleRateInt = static_cast<size_t>(std::lround(sampleRate));
    stretcher = std::make_unique<RubberBandStretcher>(sampleRateInt, 1,
        RubberBandStretcher::OptionProcessRealTime | RubberBandStretcher::OptionEngineFaster
            | RubberBandStretcher::OptionWindowShort | RubberBandStretcher::OptionPitchHighQuality
            | RubberBandStretcher::OptionThreadingNever,
        1.0, 0.5);
    stretcher->setMaxProcessSize(static_cast<size_t>(maximumBlock));
    liveShifter = std::make_unique<RubberBandLiveShifter>(sampleRateInt, 1, RubberBandLiveShifter::OptionWindowShort);
    octaver.prepare(sampleRate);

    wet.reset(ringCapacity);
    liveInput.reset(ringCapacity);
    dryDelay.assign(ringCapacity, 0.0f);
    dryPosition = 0;
    const auto scratch = static_cast<size_t>(std::max<int>(maximumBlock, static_cast<int>(liveShifter->getBlockSize())) * 4);
    scratchIn.assign(scratch, 0.0f);
    scratchOut.assign(scratch, 0.0f);
    bypassRamp.prepare(sampleRate, bypassed.load());
    mixSmoothed.reset(sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue(mix.load());
    runningMode = -1;
    active = false;
    underruns = 0;
    restartEngine(usesAnalogOctave(analogOctave.load(), semitones.load()) ? analogEngine : mode.load());
}

int LivePitchShifter::latencySamples() const noexcept
{
    return bypassed.load() ? 0 : delaySamples;
}

void LivePitchShifter::restartEngine(int activeMode) noexcept
{
    runningMode = activeMode;
    appliedSemitones = semitones.load();
    appliedScale = std::pow(2.0, appliedSemitones / 12.0);
    appliedFormant = formant.load();
    wet.read = wet.write = wet.size = 0;
    liveInput.read = liveInput.write = liveInput.size = 0;
    std::fill(dryDelay.begin(), dryDelay.end(), 0.0f);
    std::fill(scratchIn.begin(), scratchIn.end(), 0.0f);

    if (activeMode == analogEngine)
    {
        octaver.reset();
        pendingDiscard = 0;
        primed = true;
        delaySamples = 0;
    }
    else if (activeMode == static_cast<int>(Mode::lowLatency))
    {
        using RubberBand::RubberBandStretcher;
        stretcher->reset();
        stretcher->setPitchScale(appliedScale);
        stretcher->setFormantOption(appliedFormant ? RubberBandStretcher::OptionFormantPreserved : RubberBandStretcher::OptionFormantShifted);
        // Prime with the preferred start pad and drop the start delay, so the
        // output is time-aligned; the reserve below is then the whole latency.
        auto pad = static_cast<int>(stretcher->getPreferredStartPad());
        while (pad > 0)
        {
            const auto count = std::min(pad, maximumBlock);
            const float* input[] { scratchIn.data() };
            stretcher->process(input, static_cast<size_t>(count), false);
            pad -= count;
        }
        auto discard = static_cast<int>(stretcher->getStartDelay());
        for (auto available = stretcher->available(); available > 0 && discard > 0; available = stretcher->available())
        {
            const auto count = std::min({ available, discard, static_cast<int>(scratchOut.size()) });
            float* output[] { scratchOut.data() };
            discard -= static_cast<int>(stretcher->retrieve(output, static_cast<size_t>(count)));
        }
        pendingDiscard = discard;
        // The exact delay is known once output starts (see runLowLatency);
        // until then report a typical value.
        primed = false;
        inputCounter = 0;
        delaySamples = maximumBlock + lowLatencyReserve(rate, semitones.load());
    }
    else
    {
        using RubberBand::RubberBandLiveShifter;
        liveShifter->reset();
        liveShifter->setPitchScale(appliedScale);
        liveShifter->setFormantOption(appliedFormant ? RubberBandLiveShifter::OptionFormantPreserved : RubberBandLiveShifter::OptionFormantShifted);
        pendingDiscard = 0;
        primed = true;
        // One full shifter block is collected before the first output; the
        // shifter itself adds its start delay.
        delaySamples = static_cast<int>(liveShifter->getBlockSize() + liveShifter->getStartDelay());
        for (size_t sample = 0; sample < liveShifter->getBlockSize(); ++sample)
            wet.push(0.0f);
    }
}

void LivePitchShifter::runLowLatency(const float* input, int numSamples) noexcept
{
    const float* in[] { input };
    stretcher->process(in, static_cast<size_t>(numSamples), false);
    for (auto available = stretcher->available(); available > 0; available = stretcher->available())
    {
        const auto count = std::min(available, static_cast<int>(scratchOut.size()));
        float* out[] { scratchOut.data() };
        const auto got = static_cast<int>(stretcher->retrieve(out, static_cast<size_t>(count)));
        for (int sample = 0; sample < got; ++sample)
        {
            if (pendingDiscard > 0)
                --pendingDiscard;
            else if (wet.size < wet.data.size())
                wet.push(scratchOut[static_cast<size_t>(sample)]);
        }
        if (got <= 0)
            break;
    }
    // Output is time-aligned with the input (start pad dropped), so the delay
    // is exactly the input position at which playback of the queue begins.
    if (! primed && static_cast<int>(wet.size) >= numSamples + lowLatencyReserve(rate, appliedSemitones))
    {
        primed = true;
        delaySamples = static_cast<int>(inputCounter);
    }
    inputCounter += static_cast<size_t>(numSamples);
}

void LivePitchShifter::runHighQuality(const float* input, int numSamples) noexcept
{
    const auto block = liveShifter->getBlockSize();
    for (int sample = 0; sample < numSamples; ++sample)
        if (liveInput.size < liveInput.data.size())
            liveInput.push(input[sample]);
    while (liveInput.size >= block)
    {
        for (size_t sample = 0; sample < block; ++sample)
            scratchIn[sample] = liveInput.pop();
        const float* in[] { scratchIn.data() };
        float* out[] { scratchOut.data() };
        liveShifter->shift(in, out);
        for (size_t sample = 0; sample < block; ++sample)
            if (wet.size < wet.data.size())
                wet.push(scratchOut[sample]);
    }
}

void LivePitchShifter::process(float* mono, int numSamples) noexcept
{
    if (stretcher == nullptr || numSamples > maximumBlock)
        return;
    const auto isBypassed = bypassed.load();
    bypassRamp.setBypassed(isBypassed);
    if (isBypassed && ! active)
        return; // fully off: no CPU, no latency
    const auto wantedMode = usesAnalogOctave(analogOctave.load(), semitones.load()) ? analogEngine : mode.load();
    if (! active)
    {
        active = true;
        restartEngine(wantedMode); // fresh, silent start when switching on
    }

    if (wantedMode != runningMode)
        restartEngine(wantedMode);
    const auto scale = std::pow(2.0, semitones.load() / 12.0);
    const auto wantFormant = formant.load();
    if (runningMode == analogEngine)
    {
        // nothing to retune: the flip-flop octaver only does -12
    }
    else if (std::abs(scale - appliedScale) > 1.0e-9)
    {
        // The engines' delay depends on the ratio: restart so the reported
        // latency (dry alignment, recording compensation) stays exact.
        restartEngine(runningMode);
    }
    else if (wantFormant != appliedFormant)
    {
        appliedFormant = wantFormant;
        if (runningMode == static_cast<int>(Mode::lowLatency))
            stretcher->setFormantOption(wantFormant ? RubberBand::RubberBandStretcher::OptionFormantPreserved : RubberBand::RubberBandStretcher::OptionFormantShifted);
        else
            liveShifter->setFormantOption(wantFormant ? RubberBand::RubberBandLiveShifter::OptionFormantPreserved : RubberBand::RubberBandLiveShifter::OptionFormantShifted);
    }

    const auto analog = runningMode == analogEngine;
    if (runningMode == static_cast<int>(Mode::lowLatency))
        runLowLatency(mono, numSamples);
    else if (! analog)
        runHighQuality(mono, numSamples);

    mixSmoothed.setTargetValue(mix.load());
    float rampEnd = 1.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto dry = mono[sample];
        dryDelay[dryPosition] = dry;
        const auto delayed = dryDelay[(dryPosition + dryDelay.size() - static_cast<size_t>(delaySamples)) % dryDelay.size()];
        dryPosition = (dryPosition + 1) % dryDelay.size();
        auto shifted = 0.0f;
        if (analog)
            shifted = octaver.process(dry);
        else if (primed)
        {
            if (wet.size > 0)
                shifted = wet.pop();
            else
                ++underruns;
        }
        const auto amount = mixSmoothed.getNextValue();
        const auto effected = delayed + amount * (shifted - delayed);
        rampEnd = bypassRamp.nextWetMix();
        mono[sample] = dry + rampEnd * (effected - dry);
    }
    if (isBypassed && rampEnd <= 0.0f)
        active = false;
}
} // namespace nam::effects
