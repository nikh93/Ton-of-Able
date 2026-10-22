#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/** A compact labelled rotary control with an automatically updated value readout. */
class RotaryParameterControl final : public juce::Component
{
public:
    explicit RotaryParameterControl(juce::String parameterName = {});

    void setName(juce::String parameterName);
    void setRange(double minimum, double maximum, double interval = 0.0);
    void setValue(double value, juce::NotificationType notification = juce::sendNotificationSync);
    [[nodiscard]] double getValue() const noexcept;

    /** Optional human-readable formatter, e.g. return juce::String(value, 1) + " dB". */
    void setValueFormatter(std::function<juce::String(double)> formatter);
    [[nodiscard]] juce::Slider& slider() noexcept { return rotary; }
    [[nodiscard]] const juce::Slider& slider() const noexcept { return rotary; }

    std::function<void(double)> onValueChange;

    void resized() override;

private:
    void updateValueLabel();

    juce::Label nameLabel;
    juce::Slider rotary;
    juce::Label valueLabel;
    std::function<juce::String(double)> valueFormatter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryParameterControl)
};
} // namespace nam::ui
