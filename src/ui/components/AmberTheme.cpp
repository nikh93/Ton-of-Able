#include "AmberTheme.h"

namespace nam::ui
{
void StudioLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(r.translated(0, 2), 7);
    // Buttons with their own "on" colour (record arm, mute, solo) show it clearly when active.
    const auto ownOnColour = b.getToggleState() && b.isColourSpecified(juce::TextButton::buttonOnColourId);
    const auto top = ownOnColour ? b.findColour(juce::TextButton::buttonOnColourId)
                   : b.getToggleState() ? AmberTheme::amber().darker(0.35f) : juce::Colour(0xff584738);
    g.setGradientFill(juce::ColourGradient(top.brighter(over ? 0.15f : 0), 0, r.getY(),
        ownOnColour ? top.darker(0.45f) : juce::Colour(down ? 0xff211c18 : 0xff30271f), 0, r.getBottom(), false));
    g.fillRoundedRectangle(r, 7);
    g.setColour(juce::Colours::white.withAlpha(b.isEnabled() ? 0.16f : 0.05f));
    g.drawRoundedRectangle(r.reduced(0.5f), 7, 1);
}

void StudioLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float value, float start, float end, juce::Slider&)
{
    const auto size = juce::jmin(108.0f, float(juce::jmin(w, h)) - 8);
    auto r = juce::Rectangle<float>(size, size).withCentre({ x + w * .5f, y + h * .5f });
    juce::Path arc; arc.addCentredArc(r.getCentreX(), r.getCentreY(), size*.47f, size*.47f, 0, start, end, true);
    g.setColour(juce::Colour(0xff191613)); g.strokePath(arc, juce::PathStrokeType(4));
    juce::Path active; active.addCentredArc(r.getCentreX(), r.getCentreY(), size*.47f, size*.47f, 0, start, start + value*(end-start), true);
    g.setColour(AmberTheme::amber()); g.strokePath(active, juce::PathStrokeType(3));
    r.reduce(9, 9);
    g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillEllipse(r.translated(0, 4));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff766553), r.getX(), r.getY(),
        juce::Colour(0xff27231f), r.getRight(), r.getBottom(), false)); g.fillEllipse(r);
    g.setColour(juce::Colours::white.withAlpha(.2f)); g.drawEllipse(r, 1);
    auto angle = start + value*(end-start);
    auto c = r.getCentre();
    g.setColour(AmberTheme::amberBright());
    g.drawLine(c.x + std::sin(angle)*size*.20f, c.y - std::cos(angle)*size*.20f,
        c.x + std::sin(angle)*size*.31f, c.y - std::cos(angle)*size*.31f, 3);
}

void AmberTheme::applyTo(juce::LookAndFeel_V4& lookAndFeel)
{
    lookAndFeel.setColour(juce::ResizableWindow::backgroundColourId, background());
    lookAndFeel.setColour(juce::Label::textColourId, text());
    lookAndFeel.setColour(juce::TextButton::buttonColourId, surfaceRaised());
    lookAndFeel.setColour(juce::TextButton::textColourOffId, text());
    lookAndFeel.setColour(juce::Slider::thumbColourId, amber());
    lookAndFeel.setColour(juce::Slider::rotarySliderFillColourId, amber());
    lookAndFeel.setColour(juce::Slider::rotarySliderOutlineColourId, surfaceRaised());
    lookAndFeel.setColour(juce::Slider::textBoxTextColourId, text());
    lookAndFeel.setColour(juce::Slider::textBoxBackgroundColourId, background());
    lookAndFeel.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lookAndFeel.setColour(juce::ComboBox::backgroundColourId, surface());
    lookAndFeel.setColour(juce::ComboBox::textColourId, text());
    lookAndFeel.setColour(juce::ComboBox::outlineColourId, amber().withAlpha(0.45f));
}
} // namespace nam::ui
