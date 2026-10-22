#pragma once

#include <atomic>

#include "RTUtilities.h"

namespace nam::effects
{
class DriveProcessor final
{
public:
    enum class DriveType : unsigned char { Crunch, WarmOverdrive, SingingLead, Fuzz };

    void prepare(const juce::dsp::ProcessSpec&);
    void reset() noexcept;
    void process(juce::AudioBuffer<float>&) noexcept;

    void setBypassed(bool value) noexcept { bypass.setBypassed(value); }
    void setType(DriveType value) noexcept { type.store(static_cast<unsigned char>(value), std::memory_order_relaxed); }
    void setDrive(float value) noexcept { drive.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setTone(float value) noexcept { tone.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setWarmth(float value) noexcept { warmth.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setPresence(float value) noexcept { presence.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setSustain(float value) noexcept { sustain.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setBias(float value) noexcept { bias.set(juce::jlimit(0.0f, 1.0f, value)); }
    void setLevel(float value) noexcept { level.set(juce::jlimit(0.0f, 1.5f, value)); }
    void setMix(float value) noexcept { mix.set(juce::jlimit(0.0f, 1.0f, value)); }
    [[nodiscard]] int latencySamples() const noexcept { return juce::roundToInt(oversampling.getLatencyInSamples()); }

private:
    struct OnePole
    {
        float low = 0.0f;
        float processLow(float x, float coefficient) noexcept { low += coefficient * (x - low); return low; }
        float processHigh(float x, float coefficient) noexcept { return x - processLow(x, coefficient); }
        void reset() noexcept { low = 0.0f; }
    };

    float shape(float input, DriveType, float driveValue, float toneValue, float warmthValue,
                float presenceValue, float sustainValue, float biasValue) noexcept;
    static float coefficient(float hz, double rate) noexcept;

    juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    int maximumBlockSize = 0;
    double baseRate = 48000.0, oversampledRate = 192000.0;
    std::atomic<unsigned char> type { static_cast<unsigned char>(DriveType::Crunch) };
    SmoothedParameter drive { .35f }, tone { .5f }, warmth { .5f }, presence { .5f }, sustain { .5f }, bias { .35f }, level { .8f }, mix { 1.0f };
    BypassRamp bypass;
    OnePole preLow, postLow, postLow2, midLow, midHigh, interstageLow, interstageHigh, fuzzGate, bassShelf, inputLow, dcBlock;
    float leadEnvelope = 0.0f;
};
}
