#pragma once

#include "RTUtilities.h"

#include <array>
#include <cmath>

namespace nam::effects
{
class ReverbProcessor final
{
public:
    // The delay storage is allocated only while preparing, never from process().
    void prepare(const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = juce::jmax(1.0, spec.sampleRate);
        room.prepare(sampleRate, .080, .35f);
        decaySeconds.prepare(sampleRate, .080, 2.5f);
        damping.prepare(sampleRate, .080, .45f);
        preDelayMilliseconds.prepare(sampleRate, .050, 0.0f);
        lowCutHz.prepare(sampleRate, .080, 20.0f);
        highCutHz.prepare(sampleRate, .080, 18000.0f);
        mix.prepare(sampleRate, .040, .20f);
        bypass.prepare(sampleRate);

        // 250 ms is a musically useful pre-delay range and bounds all memory use.
        preDelayBuffer.setSize(2, juce::jmax(2, static_cast<int>(std::ceil(sampleRate * maxPreDelaySeconds)) + 1), false, true, true);
        reverb.setSampleRate(sampleRate);
        juce::Reverb::Parameters initial;
        initial.wetLevel = 1.0f / 3.0f; initial.dryLevel = 0.0f;
        reverb.setParameters(initial);
        reset();
    }

    void reset() noexcept
    {
        reverb.reset();
        preDelayBuffer.clear();
        preDelayWritePosition = 0;
        for (auto& state : filterStates) state = {};
    }

    // Size and decay remain separate: size controls the virtual room, decay the retained energy.
    void setRoomSize(float v) noexcept { room.set(juce::jlimit(0.0f, 1.0f, v)); }
    void setSize(float v) noexcept { setRoomSize(v); }
    void setDecay(float seconds) noexcept { decaySeconds.set(juce::jlimit(0.1f, 12.0f, seconds)); }
    void setDamping(float v) noexcept { damping.set(juce::jlimit(0.0f, 1.0f, v)); }
    void setPreDelay(float milliseconds) noexcept { preDelayMilliseconds.set(juce::jlimit(0.0f, 250.0f, milliseconds)); }
    void setLowCut(float hz) noexcept { lowCutHz.set(juce::jlimit(20.0f, 4000.0f, hz)); }
    void setHighCut(float hz) noexcept { highCutHz.set(juce::jlimit(1000.0f, 20000.0f, hz)); }
    void setMix(float v) noexcept { mix.set(juce::jlimit(0.0f, 1.0f, v)); }
    void setBypassed(bool v) noexcept { bypass.setBypassed(v); }
    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        const auto channels = block.getNumChannels();
        if (channels == 0 || preDelayBuffer.getNumSamples() == 0) return;
        auto* delayLeft = preDelayBuffer.getWritePointer(0);
        auto* delayRight = preDelayBuffer.getWritePointer(1);
        const auto delayLength = preDelayBuffer.getNumSamples();
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto delaySamples = juce::jlimit(0.0f, static_cast<float>(delayLength - 2), preDelayMilliseconds.next() * 0.001f * static_cast<float>(sampleRate));
            const int wholeDelay = static_cast<int>(delaySamples);
            const float fraction = delaySamples - static_cast<float>(wholeDelay);
            const auto readPosition = (preDelayWritePosition - wholeDelay + delayLength) % delayLength;
            const auto previousPosition = (readPosition + delayLength - 1) % delayLength;
            const auto lowCut = lowCutHz.next();
            const auto highCut = juce::jmax(lowCut + 1.0f, highCutHz.next());
            auto* left = block.getChannelPointer(0) + i;
            const float dryLeft = *left;
            delayLeft[preDelayWritePosition] = filterInput(dryLeft, filterStates[0], lowCut, highCut);
            *left = delayLeft[readPosition] + fraction * (delayLeft[previousPosition] - delayLeft[readPosition]);

            juce::Reverb::Parameters parameters;
            // Map decay seconds to comb feedback. JUCE maps roomSize to
            // 0.7 + 0.28 * roomSize; its wetLevel has an internal 3x gain.
            const float size = room.next();
            const float decay = decaySeconds.next();
            const float feedback = std::pow(0.001f, (.026f + .012f * size) / decay);
            parameters.roomSize = juce::jlimit(0.0f, 1.0f, (feedback - .7f) / .28f);
            parameters.damping = damping.next(); parameters.wetLevel = 1.0f / 3.0f; parameters.dryLevel = 0.0f; parameters.width = 1.0f; parameters.freezeMode = 0.0f;
            reverb.setParameters(parameters);

            float dryRight = dryLeft;
            float* right = nullptr;
            if (channels > 1)
            {
                right = block.getChannelPointer(1) + i;
                dryRight = *right;
                delayRight[preDelayWritePosition] = filterInput(dryRight, filterStates[1], lowCut, highCut);
                *right = delayRight[readPosition] + fraction * (delayRight[previousPosition] - delayRight[readPosition]);
                reverb.processStereo(left, right, 1);
            }
            else reverb.processMono(left, 1);

            const auto wetMix = mix.next() * bypass.nextWetMix();
            *left = dryLeft + wetMix * (*left - dryLeft);
            if (right != nullptr) *right = dryRight + wetMix * (*right - dryRight);
            preDelayWritePosition = (preDelayWritePosition + 1) % delayLength;
        }
    }
    void process(juce::AudioBuffer<float>& b) noexcept { process(juce::dsp::AudioBlock<float>(b)); }
private:
    struct FilterState { float hpPreviousInput = 0.0f, hpPreviousOutput = 0.0f, lpPreviousOutput = 0.0f; };
    float filterInput(float input, FilterState& state, float lowCut, float highCut) const noexcept
    {
        constexpr auto twoPi = 6.28318530717958647692f;
        const auto sampleInterval = 1.0f / static_cast<float>(sampleRate);
        const auto hpRc = 1.0f / (twoPi * lowCut);
        const auto highPassed = (hpRc / (hpRc + sampleInterval)) * (state.hpPreviousOutput + input - state.hpPreviousInput);
        state.hpPreviousInput = input; state.hpPreviousOutput = highPassed;
        const auto lpRc = 1.0f / (twoPi * highCut);
        state.lpPreviousOutput += (sampleInterval / (lpRc + sampleInterval)) * (highPassed - state.lpPreviousOutput);
        return state.lpPreviousOutput;
    }

    static constexpr double maxPreDelaySeconds = 0.250;
    double sampleRate = 44100.0;
    juce::Reverb reverb;
    juce::AudioBuffer<float> preDelayBuffer;
    int preDelayWritePosition = 0;
    std::array<FilterState, 2> filterStates {};
    SmoothedParameter room, decaySeconds, damping, preDelayMilliseconds, lowCutHz, highCutHz, mix;
    BypassRamp bypass;
};
} // namespace nam::effects
