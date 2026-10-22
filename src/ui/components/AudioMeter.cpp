#include "AudioMeter.h"

#include <cmath>

#include "AmberTheme.h"

namespace nam::ui
{
AudioMeter::AudioMeter(const std::atomic<float>* peakSource)
{
    setPeakSource(peakSource);
    startTimerHz(30);
}

AudioMeter::~AudioMeter() { stopTimer(); }

void AudioMeter::setPeakSource(const std::atomic<float>* peakSource) noexcept
{
    source.store(peakSource, std::memory_order_release);
}

void AudioMeter::setLabel(juce::String newLabel)
{
    label = std::move(newLabel);
    repaint();
}

void AudioMeter::setDecayRate(const float decibelsPerSecond) noexcept
{
    const auto linearDecay = 1.0f - std::pow(10.0f, -juce::jmax(1.0f, decibelsPerSecond) / 20.0f);
    decayPerTick = juce::jlimit(0.001f, 1.0f, linearDecay / 30.0f);
}

void AudioMeter::clearClipIndicator() noexcept
{
    clipping = false;
    clipHoldTicks = 0;
    repaint();
}

void AudioMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff343c36), 0, 0, juce::Colour(0xff121915), 0, float(getHeight()), false));
    g.fillRoundedRectangle(r, 8);
    g.setColour(juce::Colours::white.withAlpha(.13f)); g.drawRoundedRectangle(r, 8, 1);
    auto db = [](float p) { return 20.0f * std::log10(juce::jmax(.000001f, p)); };
    g.setFont(juce::FontOptions(10, juce::Font::bold)); g.setColour(AmberTheme::subduedText());
    auto text = getLocalBounds().reduced(9,4).removeFromTop(16);
    g.drawText(label, text.removeFromLeft(48), juce::Justification::left);
    g.drawText(juce::String(db(displayedPeak),1) + " dB  /  PK " + juce::String(db(heldPeak),1), text, juce::Justification::centred);
    auto bar = getLocalBounds().reduced(10).withTrimmedTop(14).withHeight(7);
    for (int i=0; i<32; ++i)
    {
        g.setColour(i < int(32*juce::jlimit(0.f,1.f,(db(displayedPeak)+60)/60)) ?
            (i>28 ? AmberTheme::danger() : AmberTheme::amber()) : juce::Colour(0xff41473b));
        g.fillRect(bar.getX()+i*bar.getWidth()/32, bar.getY(), juce::jmax(1,bar.getWidth()/32-2), 7);
    }
    if (clipping) { g.setColour(AmberTheme::danger()); g.drawText("CLIP", getLocalBounds().removeFromRight(40), juce::Justification::centred); }
}
void AudioMeter::resized() {}

void AudioMeter::timerCallback()
{
    const auto* peakSource = source.load(std::memory_order_acquire);
    const auto peak = peakSource != nullptr ? peakSource->load(std::memory_order_relaxed) : 0.0f;
    if (peak >= 1.0f)
    {
        clipping = true;
        clipHoldTicks = 45;
    }
    else if (clipHoldTicks > 0 && --clipHoldTicks == 0)
    {
        clipping = false;
    }
    heldPeak = juce::jmax(peak, heldPeak - decayPerTick * 0.18f);
    displayedPeak = juce::jmax(juce::jlimit(0.0f, 1.0f, peak), displayedPeak - decayPerTick);
    repaint();
}
} // namespace nam::ui
