#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/** Flat, modern controls: rounded buttons without bevels that join into
    segmented groups (setConnectedEdges), slim sliders and scroll bars,
    quiet combo boxes, menus and tooltips. Toggled buttons fill with the
    accent colour (or their own buttonOnColourId). */
class StudioLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StudioLookAndFeel();

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int, int, int, int, bool, int, int, bool, bool) override;
    int getDefaultScrollbarWidth() override { return 10; }
    void drawTooltip(juce::Graphics&, const juce::String&, int, int) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTickBox(juce::Graphics&, juce::Component&, float, float, float, float, bool, bool, bool, bool) override;
    void fillTextEditorBackground(juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int, int, juce::TextEditor&) override;

    /** The rounded outline of a control, square where it joins a neighbour. */
    static juce::Path controlShape(juce::Rectangle<float>, float radius, int connectedEdges);
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

    // Flat control surfaces.
    static juce::Colour panel() noexcept { return juce::Colour { 0xff1f1a16 }; }      // toolbars, headers
    static juce::Colour control() noexcept { return juce::Colour { 0xff2d2620 }; }    // button face
    static juce::Colour well() noexcept { return juce::Colour { 0xff14110e }; }       // displays, tracks
    static juce::Colour outline() noexcept { return juce::Colours::white.withAlpha(0.08f); }
    static juce::Colour onText() noexcept { return juce::Colour { 0xff1c150d }; }     // text on accent fills
    static juce::Colour record() noexcept { return juce::Colour { 0xffe5534b }; }

    /** Applies the palette to standard JUCE widgets in the supplied LookAndFeel. */
    static void applyTo(juce::LookAndFeel_V4& lookAndFeel);

private:
    AmberTheme() = delete;
};
} // namespace nam::ui
