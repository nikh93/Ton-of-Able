#pragma once

#include <array>
#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/** Ten vertical band sliders with a live frequency-response curve for
    nam::effects::GraphicEQProcessor. */
class GraphicEqComponent final : public juce::Component
{
public:
    using Bands = std::array<float, 10>;

    GraphicEqComponent();
    void setValues(bool bypassed, const Bands& db);
    [[nodiscard]] bool isBypassed() const noexcept { return ! power.getToggleState(); }
    [[nodiscard]] Bands values() const noexcept { return gains; }

    std::function<void(bool bypassed, const Bands& db)> onChange;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void publish();
    juce::Rectangle<int> curveArea() const;

    juce::ToggleButton power { "GRAPHIC EQ" };
    juce::ComboBox presets;
    std::array<juce::Slider, 10> sliders;
    std::array<juce::Label, 10> labels;
    Bands gains {};
};
} // namespace nam::ui
