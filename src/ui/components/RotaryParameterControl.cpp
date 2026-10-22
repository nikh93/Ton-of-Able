#include "RotaryParameterControl.h"

#include "AmberTheme.h"

namespace nam::ui
{
RotaryParameterControl::RotaryParameterControl(juce::String parameterName)
{
    nameLabel.setText(std::move(parameterName), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId, AmberTheme::text());
    addAndMakeVisible(nameLabel);

    rotary.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    rotary.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    rotary.setRange(0.0, 1.0);
    rotary.onValueChange = [this]
    {
        updateValueLabel();
        if (onValueChange)
            onValueChange(rotary.getValue());
    };
    addAndMakeVisible(rotary);

    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setFont(juce::FontOptions(11.0f));
    valueLabel.setColour(juce::Label::textColourId, AmberTheme::amberBright());
    addAndMakeVisible(valueLabel);
    updateValueLabel();
}

void RotaryParameterControl::setName(juce::String parameterName)
{
    nameLabel.setText(std::move(parameterName), juce::dontSendNotification);
}

void RotaryParameterControl::setRange(const double minimum, const double maximum, const double interval)
{
    rotary.setRange(minimum, maximum, interval);
    updateValueLabel();
}

void RotaryParameterControl::setValue(const double value, const juce::NotificationType notification)
{
    rotary.setValue(value, notification);
    if (notification == juce::dontSendNotification)
        updateValueLabel();
}

double RotaryParameterControl::getValue() const noexcept { return rotary.getValue(); }

void RotaryParameterControl::setValueFormatter(std::function<juce::String(double)> formatter)
{
    valueFormatter = std::move(formatter);
    updateValueLabel();
}

void RotaryParameterControl::resized()
{
    auto area = getLocalBounds();
    nameLabel.setBounds(area.removeFromTop(18));
    valueLabel.setBounds(area.removeFromBottom(18));
    rotary.setBounds(area.reduced(2));
}

void RotaryParameterControl::updateValueLabel()
{
    const auto value = rotary.getValue();
    valueLabel.setText(valueFormatter ? valueFormatter(value) : juce::String(value, 2), juce::dontSendNotification);
}
} // namespace nam::ui
