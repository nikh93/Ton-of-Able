#pragma once

#include "RTUtilities.h"

namespace nam::effects
{
class ChorusProcessor final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) { sampleRate=spec.sampleRate; maxSamples=(int)std::ceil(sampleRate*.060)+2; delay.setSize((int)juce::jmin<size_t>(2,spec.numChannels),maxSamples,false,false,true); rate.prepare(sampleRate,.050,.35f); depth.prepare(sampleRate,.050,.45f); centerDelay.prepare(sampleRate,.050,18.0f); feedback.prepare(sampleRate,.050,0.0f); mix.prepare(sampleRate,.030,.35f); bypass.prepare(sampleRate); reset(); }
    void reset() noexcept { delay.clear(); write=0; phase=0; }
    void setRateHz(float v) noexcept { rate.set(juce::jlimit(.05f,8.0f,v)); }
    // Depth is normalised and modulates the centre delay by up to 20 ms.
    void setDepth(float v) noexcept { depth.set(juce::jlimit(0.0f,1.0f,v)); }
    void setCenterDelayMs(float v) noexcept { centerDelay.set(juce::jlimit(1.0f,45.0f,v)); }
    void setFeedback(float v) noexcept { feedback.set(juce::jlimit(-0.95f,0.95f,v)); }
    void setMix(float v) noexcept { mix.set(juce::jlimit(0.0f,1.0f,v)); }
    void setBypassed(bool v) noexcept { bypass.setBypassed(v); }
    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        const int channels = (int) juce::jmin<size_t>(2, block.getNumChannels());
        if (maxSamples == 0) return;
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float centreMs = centerDelay.next();
            const float excursionMs = juce::jmin(depth.next() * 20.f, centreMs - .1f);
            const float delayMs = centreMs + excursionMs * std::sin(phase);
            const float d = juce::jlimit(1.f, (float) maxSamples - 2.f, delayMs * .001f * (float) sampleRate);
            phase += juce::MathConstants<float>::twoPi * rate.next() / (float) sampleRate;
            if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
            const int readA = (write - (int) d + maxSamples) % maxSamples;
            const int readB = (readA - 1 + maxSamples) % maxSamples;
            const float fraction = d - std::floor(d), wetMix = mix.next() * bypass.nextWetMix(), feedbackGain = feedback.next();
            for (int ch = 0; ch < channels; ++ch)
            {
                auto* p = block.getChannelPointer((size_t) ch);
                const float dry = p[i], delayed = delay.getSample(ch, readA) * (1 - fraction) + delay.getSample(ch, readB) * fraction;
                delay.setSample(ch, write, dry + delayed * feedbackGain);
                p[i] = dry + wetMix * (delayed - dry);
            }
            write = (write + 1) % maxSamples;
        }
    }
    void process(juce::AudioBuffer<float>& b) noexcept { process(juce::dsp::AudioBlock<float>(b)); }
private: double sampleRate=44100; int maxSamples=0,write=0; float phase=0; juce::AudioBuffer<float> delay; SmoothedParameter rate,depth,centerDelay,feedback,mix; BypassRamp bypass;
};
} // namespace nam::effects
