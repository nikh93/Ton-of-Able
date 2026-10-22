#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
class StudioLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};
/** Shared warm-amber palette and LookAndFeel setup for the application's UI. */
class AmberTheme final
{
public:
    static juce::Colour background() noexcept { return juce::Colour { 0xff17130f }; }
    static juce::Colour surface() noexcept { return juce::Colour { 0xff292018 }; }
    static juce::Colour surfaceRaised() noexcept { return juce::Colour { 0xff3a2b1e }; }
    static juce::Colour amber() noexcept { return juce::Colour { 0xffe3a447 }; }
    static juce::Colour amberBright() noexcept { return juce::Colour { 0xffffc66d }; }
    static juce::Colour text() noexcept { return juce::Colour { 0xfff1dfc4 }; }
    static juce::Colour subduedText() noexcept { return juce::Colour { 0xffbcaa91 }; }
    static juce::Colour danger() noexcept { return juce::Colour { 0xffdd735c }; }

    /** Applies the palette to standard JUCE widgets in the supplied LookAndFeel. */
    static void applyTo(juce::LookAndFeel_V4& lookAndFeel);

private:
    AmberTheme() = delete;
};
} // namespace nam::ui
