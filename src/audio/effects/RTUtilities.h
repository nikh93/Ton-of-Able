#pragma once

#include <atomic>
#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace nam::effects
{
class SmoothedParameter final
{
public:
    explicit SmoothedParameter(float initial = 0.0f) noexcept : target(initial) {}

    void prepare(double sampleRate, double rampSeconds, float initial) noexcept
    {
        if (prepared) initial = target.load(std::memory_order_relaxed);
        target.store(initial, std::memory_order_relaxed);
        prepared = true;
        smoother.reset(sampleRate, rampSeconds);
        smoother.setCurrentAndTargetValue(initial);
        cachedTarget = initial;
    }

    void set(float value) noexcept { target.store(value, std::memory_order_relaxed); }

    float next() noexcept
    {
        const auto requested = target.load(std::memory_order_relaxed);
        if (requested != cachedTarget)
        {
            cachedTarget = requested;
            smoother.setTargetValue(requested);
        }
        return smoother.getNextValue();
    }

private:
    std::atomic<float> target;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoother;
    float cachedTarget = 0.0f;
    bool prepared = false;
};

class BypassRamp final
{
public:
    void prepare(double sampleRate, bool initiallyBypassed = false) noexcept
    {
        if (prepared) initiallyBypassed = bypassed.load(std::memory_order_relaxed);
        bypassed.store(initiallyBypassed, std::memory_order_relaxed);
        prepared = true;
        mix.reset(sampleRate, 0.010);
        mix.setCurrentAndTargetValue(initiallyBypassed ? 0.0f : 1.0f);
        cachedBypass = initiallyBypassed;
    }

    void setBypassed(bool shouldBypass) noexcept { bypassed.store(shouldBypass, std::memory_order_relaxed); }

    float nextWetMix() noexcept
    {
        const auto requested = bypassed.load(std::memory_order_relaxed);
        if (requested != cachedBypass)
        {
            cachedBypass = requested;
            mix.setTargetValue(requested ? 0.0f : 1.0f);
        }
        return mix.getNextValue();
    }

    /** Audio thread: bypassed and the fade-out has finished, so processing can be skipped. */
    [[nodiscard]] bool isFullyBypassed() const noexcept
    {
        return bypassed.load(std::memory_order_relaxed) && cachedBypass && ! mix.isSmoothing();
    }

private:
    std::atomic<bool> bypassed { false };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix;
    bool cachedBypass = false;
    bool prepared = false;
};

inline float decibelsToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
} // namespace nam::effects
