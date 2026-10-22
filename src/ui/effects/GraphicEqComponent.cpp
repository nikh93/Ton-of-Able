#include "GraphicEqComponent.h"

#include <complex>

#include "audio/effects/GraphicEQProcessor.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
namespace
{
using Theme = AmberTheme;
using Processor = nam::effects::GraphicEQProcessor;

struct Preset { const char* name; GraphicEqComponent::Bands db; };
const Preset presetList[] {
    { "Flat", {} },
    { "Scoop (Metal)", { 2.0f, 3.0f, 2.0f, 0.0f, -3.0f, -5.0f, -3.0f, 1.0f, 3.0f, 2.0f } },
    { "Mid Boost (Lead)", { -2.0f, -1.0f, 0.0f, 2.0f, 4.0f, 5.0f, 3.0f, 1.0f, -1.0f, -2.0f } },
    { "Tight Low End", { -9.0f, -5.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    { "Presence", { 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 3.0f, 4.0f, 2.0f, 0.0f } },
    { "Warm / Dark", { 1.0f, 2.0f, 2.0f, 1.0f, 0.0f, -1.0f, -2.0f, -4.0f, -6.0f, -8.0f } },
    { "Mix: Cut Mud", { -4.0f, -1.0f, -2.0f, -4.0f, -2.0f, 0.0f, 1.0f, 1.0f, 0.0f, -2.0f } },
};

juce::String bandName(float frequency)
{
    return frequency >= 1000.0f ? juce::String(frequency / 1000.0f, 0) + "k" : juce::String(frequency, 0);
}
} // namespace

GraphicEqComponent::GraphicEqComponent()
{
    power.setColour(juce::ToggleButton::textColourId, Theme::text());
    power.onClick = [this] { repaint(); publish(); };
    addAndMakeVisible(power);

    for (size_t index = 0; index < std::size(presetList); ++index)
        presets.addItem(presetList[index].name, static_cast<int>(index) + 1);
    presets.setTextWhenNothingSelected("Preset");
    presets.onChange = [this]
    {
        const auto index = presets.getSelectedItemIndex();
        if (index < 0)
            return;
        setValues(false, presetList[static_cast<size_t>(index)].db);
        presets.setSelectedItemIndex(-1, juce::dontSendNotification);
        presets.setText("Preset", juce::dontSendNotification);
        publish();
    };
    addAndMakeVisible(presets);

    for (size_t band = 0; band < sliders.size(); ++band)
    {
        auto& slider = sliders[band];
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(-Processor::maximumGainDb, Processor::maximumGainDb, 0.1);
        slider.setDoubleClickReturnValue(true, 0.0);
        slider.setPopupDisplayEnabled(true, true, nullptr);
        slider.textFromValueFunction = [](double v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; };
        slider.setColour(juce::Slider::trackColourId, Theme::amber().withAlpha(0.6f));
        slider.onValueChange = [this, band]
        {
            gains[band] = static_cast<float>(sliders[band].getValue());
            if (! power.getToggleState())
                power.setToggleState(true, juce::dontSendNotification);
            repaint();
            publish();
        };
        addAndMakeVisible(slider);
        auto& label = labels[band];
        label.setText(bandName(Processor::frequencies[band]), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(10.5f));
        label.setColour(juce::Label::textColourId, Theme::subduedText());
        addAndMakeVisible(label);
    }
}

void GraphicEqComponent::setValues(bool bypassed, const Bands& db)
{
    gains = db;
    power.setToggleState(! bypassed, juce::dontSendNotification);
    for (size_t band = 0; band < sliders.size(); ++band)
        sliders[band].setValue(db[band], juce::dontSendNotification);
    repaint();
}

void GraphicEqComponent::publish()
{
    if (onChange)
        onChange(! power.getToggleState(), gains);
}

juce::Rectangle<int> GraphicEqComponent::curveArea() const
{
    return getLocalBounds().reduced(8).withTrimmedTop(30).removeFromTop(std::max(40, getHeight() / 3));
}

void GraphicEqComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff1d1814));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    const auto area = curveArea().toFloat();
    g.setColour(juce::Colour(0xff15110d));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(static_cast<int>(area.getCentreY()), area.getX(), area.getRight());

    // Magnitude response of the ten peaking sections, evaluated on a log axis.
    constexpr double rate = 48000.0;
    std::array<std::array<float, 6>, 10> coefficients {};
    for (size_t band = 0; band < coefficients.size(); ++band)
    {
        coefficients[band] = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(rate, Processor::frequencies[band], 1.41f, juce::Decibels::decibelsToGain(gains[band]));
    }
    juce::Path curve;
    const auto points = static_cast<int>(area.getWidth());
    for (int x = 0; x <= points; ++x)
    {
        const auto frequency = 20.0 * std::pow(1000.0, static_cast<double>(x) / std::max(1, points));
        const std::complex<double> z = std::polar(1.0, -2.0 * juce::MathConstants<double>::pi * frequency / rate);
        double db = 0.0;
        for (const auto& c : coefficients)
        {
            const auto numerator = static_cast<double>(c[0]) + static_cast<double>(c[1]) * z + static_cast<double>(c[2]) * z * z;
            const auto denominator = static_cast<double>(c[3]) + static_cast<double>(c[4]) * z + static_cast<double>(c[5]) * z * z;
            db += 20.0 * std::log10(std::max(1.0e-9, std::abs(numerator / denominator)));
        }
        const auto y = area.getCentreY() - static_cast<float>(db / (Processor::maximumGainDb * 1.6)) * area.getHeight() * 0.5f;
        if (x == 0)
            curve.startNewSubPath(area.getX(), y);
        else
            curve.lineTo(area.getX() + static_cast<float>(x), juce::jlimit(area.getY(), area.getBottom(), y));
    }
    g.setColour(power.getToggleState() ? Theme::amberBright() : Theme::subduedText().withAlpha(0.5f));
    g.strokePath(curve, juce::PathStrokeType(1.8f));
}

void GraphicEqComponent::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto top = area.removeFromTop(24);
    power.setBounds(top.removeFromLeft(130));
    presets.setBounds(top.removeFromRight(150));
    area.removeFromTop(6);
    area.removeFromTop(curveArea().getHeight() + 6);
    auto labelRow = area.removeFromBottom(16);
    const auto width = area.getWidth() / static_cast<int>(sliders.size());
    for (size_t band = 0; band < sliders.size(); ++band)
    {
        sliders[band].setBounds(area.removeFromLeft(width).reduced(2, 0));
        labels[band].setBounds(labelRow.removeFromLeft(width));
    }
}
} // namespace nam::ui
