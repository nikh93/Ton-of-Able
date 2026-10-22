#include "DriveProcessor.h"

#include <cmath>

namespace nam::effects
{
float DriveProcessor::coefficient(float hz, double rate) noexcept
{
    return juce::jlimit(0.0001f, 0.9999f, 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * hz / static_cast<float>(rate)));
}

void DriveProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    baseRate = spec.sampleRate;
    oversampledRate = baseRate * 4.0;
    oversampling.initProcessing(spec.maximumBlockSize);
    maximumBlockSize = static_cast<int>(spec.maximumBlockSize);
    // These parameters are consumed in the 4x processing loop.  Preparing them
    // at that rate keeps the audible control ramp at 18 ms rather than 4.5 ms.
    for (auto* p : { &drive, &tone, &warmth, &presence, &sustain, &bias })
        p->prepare(oversampledRate, 0.018, 0.5f);
    level.prepare(oversampledRate, 0.018, .8f);
    mix.prepare(oversampledRate, 0.018, 1.0f);
    bypass.prepare(oversampledRate, true);
    reset();
}

void DriveProcessor::reset() noexcept
{
    preLow.reset(); postLow.reset(); postLow2.reset(); midLow.reset(); midHigh.reset();
    interstageLow.reset(); interstageHigh.reset(); fuzzGate.reset(); bassShelf.reset();
    inputLow.reset(); dcBlock.reset(); leadEnvelope = 0.0f;
    oversampling.reset();
}

float DriveProcessor::shape(float x, DriveType selected, float d, float t, float w, float p, float s, float b) noexcept
{
    const auto hp = coefficient(selected == DriveType::WarmOverdrive ? 40.0f : selected == DriveType::SingingLead ? 65.0f : 70.0f, oversampledRate);
    x = preLow.processHigh(x, hp);
    const auto mid = midLow.processLow(x, coefficient(1700.0f, oversampledRate)) - midHigh.processLow(x, coefficient(550.0f, oversampledRate));
    float y = x;
    switch (selected)
    {
        case DriveType::Crunch:
        {
            const float gain = 1.0f + d * 5.0f;
            // Continuous at zero: separate positive/negative offsets caused a
            // step at every zero crossing, even for almost silent input.
            const float offset = .08f * d;
            y = std::tanh(x * gain + offset) - std::tanh(offset);
            y *= .70f + .10f * d;
            break;
        }
        case DriveType::WarmOverdrive:
        {
            // Digital pedal reference: 0.20 peak (-14 dBFS) is one internal
            // signal unit. This is gain staging, not analog dBu calibration.
            // Shelve bass BEFORE clipping; keep some clean fundamentals.
            const float bass = bassShelf.processLow(x, coefficient(420.f, oversampledRate));
            const float voiced = inputLow.processLow(x - (.65f - .25f*w)*bass + (.25f+.3f*w)*mid,
                                                     coefficient(4200.f, oversampledRate));
            const float gain = std::pow(10.f, (3.f + 27.f*d) / 20.f);
            const float offset = .12f + .13f*w;
            const float clipped = std::tanh(voiced * gain / .20f + offset) - std::tanh(offset);
            // Fixed output reference lets Level push NAM deliberately, while
            // Drive controls saturation without an unbounded output boost.
            y = .20f * clipped + (.18f - .10f*d) * x;
            break;
        }
        case DriveType::SingingLead:
        {
            const float bass = bassShelf.processLow(x, coefficient(330.f, oversampledRate));
            const float voiced = inputLow.processLow(x - .68f*bass + .5f*mid,
                                                     coefficient(3600.f, oversampledRate));
            const float magnitude = std::abs(voiced);
            const float seconds = magnitude > leadEnvelope ? .008f : .180f + .140f*s;
            leadEnvelope += (1.f - std::exp(-1.f / (static_cast<float>(oversampledRate)*seconds)))
                            * (magnitude - leadEnvelope);
            // Bounded, slow lift of a fading note (max +9.5 dB). No gate,
            // reciprocal silence gain, or sample-rate-dependent peak clamp.
            const float lift = 1.f + s*(juce::jlimit(1.f, 3.f,
                               std::pow(.065f / juce::jmax(.008f, leadEnvelope), .5f))-1.f);
            const float gain = std::pow(10.f, (5.f+20.f*d)/20.f);
            const float stageOne = std::tanh(voiced*lift*gain/.20f + .16f) - std::tanh(.16f);
            const float coupled = interstageHigh.processHigh(stageOne, coefficient(55.f, oversampledRate));
            const float interstage = interstageLow.processLow(coupled, coefficient(2300.f+1600.f*t, oversampledRate));
            // Gentler second stage, with opposite asymmetry and controlled
            // bandwidth. Presence acts after saturation via the tone filters.
            y = .24f * (std::tanh(interstage*(1.15f+1.3f*d)-.10f) + std::tanh(.10f));
            break;
        }
        case DriveType::Fuzz:
        {
            const float biased = x * (3.0f + d * 13.0f) + (b - .5f) * (.22f + b * .55f);
            const float idle = (b - .5f) * (.22f + b * .55f);
            const float soft = std::tanh(biased * (1.1f + d * 1.9f)) - std::tanh(idle * (1.1f + d * 1.9f));
            const float hard = juce::jlimit(-.82f, .82f, biased * .42f) - juce::jlimit(-.82f, .82f, idle * .42f);
            y = .46f * soft + .54f * hard;
            const float gate = fuzzGate.processLow(std::abs(x), coefficient(22.0f, oversampledRate));
            if (b > .62f && gate < (b - .62f) * .075f) y *= gate / juce::jmax(.001f, (b - .62f) * .075f);
            y *= .58f;
            break;
        }
    }
    const float lowPass = selected == DriveType::WarmOverdrive ? 2400.f + t*4800.f - w*700.f
                        : selected == DriveType::SingingLead ? 2200.f + t*3000.f + p*1200.f
                        : selected == DriveType::Fuzz ? 2400.f + t*6500.f : 3500.f + t*7500.f;
    y = postLow.processLow(y, coefficient(lowPass, oversampledRate));
    if (selected == DriveType::WarmOverdrive || selected == DriveType::SingingLead)
        y = postLow2.processLow(y, coefficient(lowPass, oversampledRate));
    return dcBlock.processHigh(y, coefficient(12.f, oversampledRate));
}

void DriveProcessor::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const auto samples = buffer.getNumSamples();
    if (buffer.getNumChannels() != 1 || samples <= 0 || samples > maximumBlockSize) return;
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto up = oversampling.processSamplesUp(block);
    const auto selected = static_cast<DriveType>(type.load(std::memory_order_relaxed));
    for (size_t i = 0; i < up.getNumSamples(); ++i)
    {
        const float d = drive.next(), t = tone.next(), w = warmth.next(), p = presence.next(), su = sustain.next(), bi = bias.next();
        const float dry = up.getChannelPointer(0)[i];
        const float effected = shape(dry, selected, d, t, w, p, su, bi) * level.next();
        const float wet = mix.next() * bypass.nextWetMix();
        // Mix inside the oversampled path: dry and wet share converter latency.
        up.getChannelPointer(0)[i] = dry + wet * (effected - dry);
    }
    oversampling.processSamplesDown(block);
}
}
