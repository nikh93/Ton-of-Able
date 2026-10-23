#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/** Vector icons drawn in code, so they look the same on every system
    (font glyphs such as ▶ or ⏮ differ or are missing on Windows). */
enum class Icon
{
    none,
    play,
    stop,
    record,
    toStart,
    loop,
    metronome,
    countIn,
    pointer,
    scissors,
    stretch,
    zoomIn,
    zoomOut,
    zoomFit,
    undo,
    redo,
    plus,
    importFile,
    exportFile,
    folder,
    keyboard,
    snowflake,
    headphones,
    sliders
};

/** Draws an icon centred in the area with the given colour. */
void drawIcon(juce::Graphics&, Icon, juce::Rectangle<float> area, juce::Colour);

/** A button with a vector icon and an optional text label, drawn with the
    app's LookAndFeel background (flat, rounded, segment-aware). */
class IconButton : public juce::TextButton
{
public:
    explicit IconButton(Icon icon = Icon::none, const juce::String& label = {});

    void setIcon(Icon);
    [[nodiscard]] Icon icon() const noexcept { return shape; }
    /** Colour of the icon when the button is off (defaults to the text colour). */
    void setIconColour(juce::Colour);

    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon shape;
    std::optional<juce::Colour> iconColour;
};
} // namespace nam::ui
