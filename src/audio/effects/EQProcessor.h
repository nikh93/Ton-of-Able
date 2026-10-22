#pragma once

#include "RTUtilities.h"

namespace nam::effects
{
// Three serial RBJ sections: low shelf, bell, and high shelf.
class EQProcessor final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) noexcept
    {
        sampleRate = spec.sampleRate;
        lowGain.prepare(sampleRate, 0.020, 0.0f); midGain.prepare(sampleRate, 0.020, 0.0f); highGain.prepare(sampleRate, 0.020, 0.0f);
        bypass.prepare(sampleRate); reset();
    }
    void reset() noexcept { for (auto& section : sections) section = {}; }
    void setLowGainDb(float v) noexcept { lowGain.set(juce::jlimit(-18.0f, 18.0f, v)); }
    void setMidGainDb(float v) noexcept { midGain.set(juce::jlimit(-18.0f, 18.0f, v)); }
    void setHighGainDb(float v) noexcept { highGain.set(juce::jlimit(-18.0f, 18.0f, v)); }
    void setBypassed(bool v) noexcept { bypass.setBypassed(v); }
    void process(juce::dsp::AudioBlock<float> block) noexcept
    {
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const auto l = lowGain.next(), m = midGain.next(), h = highGain.next();
            updateCoefficients(l, m, h);
            const auto wet = bypass.nextWetMix();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch) { const auto dry = block.getChannelPointer(ch)[i]; const auto effected = sections[ch < 2 ? ch : 0].process(dry); block.getChannelPointer(ch)[i] = dry + wet * (effected - dry); }
        }
    }
    void process(juce::AudioBuffer<float>& b) noexcept { process(juce::dsp::AudioBlock<float>(b)); }
private:
    struct Biquad { float x1{}, x2{}, y1{}, y2{}; float b0{1}, b1{}, b2{}, a1{}, a2{}; float process(float x) noexcept { const float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2; x2=x1; x1=x; y2=y1; y1=y; return y; } };
    struct Channel { Biquad low, mid, high; float process(float x) noexcept { return high.process(mid.process(low.process(x))); } };
    static void setBell(Biquad& b, float frequency, float gain, double sr) noexcept { const float a=std::pow(10.0f,gain/40.0f), w=2.0f*juce::MathConstants<float>::pi*frequency/(float)sr, alpha=std::sin(w)/(2.0f*0.85f); const float n=1+alpha/a; b.b0=(1+alpha*a)/n; b.b1=(-2*std::cos(w))/n; b.b2=(1-alpha*a)/n; b.a1=b.b1; b.a2=(1-alpha/a)/n; }
    static void setShelf(Biquad& b, float frequency, float gain, bool high, double sr) noexcept { const float a=std::pow(10.0f,gain/40.0f), w=2.0f*juce::MathConstants<float>::pi*frequency/(float)sr, c=std::cos(w), alpha=std::sin(w)*0.5f*std::sqrt((a+1/a)*1.0f); const float two=2*std::sqrt(a)*alpha; float b0,b1,b2,a0,a1,a2; if(high){b0=a*((a+1)+(a-1)*c+two);b1=-2*a*((a-1)+(a+1)*c);b2=a*((a+1)+(a-1)*c-two);a0=(a+1)-(a-1)*c+two;a1=2*((a-1)-(a+1)*c);a2=(a+1)-(a-1)*c-two;}else{b0=a*((a+1)-(a-1)*c+two);b1=2*a*((a-1)-(a+1)*c);b2=a*((a+1)-(a-1)*c-two);a0=(a+1)+(a-1)*c+two;a1=-2*((a-1)+(a+1)*c);a2=(a+1)+(a-1)*c-two;} b.b0=b0/a0;b.b1=b1/a0;b.b2=b2/a0;b.a1=a1/a0;b.a2=a2/a0; }
    void updateCoefficients(float l,float m,float h) noexcept { for(auto& c:sections){setShelf(c.low,120,l,false,sampleRate);setBell(c.mid,800,m,sampleRate);setShelf(c.high,4000,h,true,sampleRate);} }
    double sampleRate = 44100.0; Channel sections[2]; SmoothedParameter lowGain, midGain, highGain; BypassRamp bypass;
};
} // namespace nam::effects
