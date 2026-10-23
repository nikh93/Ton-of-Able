#include "AmberTheme.h"

namespace nam::ui
{
using Theme = AmberTheme;

StudioLookAndFeel::StudioLookAndFeel() = default;

juce::Path StudioLookAndFeel::controlShape(juce::Rectangle<float> r, float radius, int connectedEdges)
{
    const auto left = (connectedEdges & juce::Button::ConnectedOnLeft) == 0;
    const auto right = (connectedEdges & juce::Button::ConnectedOnRight) == 0;
    const auto top = (connectedEdges & juce::Button::ConnectedOnTop) == 0;
    const auto bottom = (connectedEdges & juce::Button::ConnectedOnBottom) == 0;
    juce::Path path;
    path.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius,
                             left && top, right && top, left && bottom, right && bottom);
    return path;
}

void StudioLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const auto radius = std::min(6.0f, bounds.getHeight() * 0.5f);
    const auto shape = controlShape(bounds.reduced(0.5f), radius, b.getConnectedEdgeFlags());

    const auto on = b.getToggleState();
    auto fill = on ? (b.isColourSpecified(juce::TextButton::buttonOnColourId) ? b.findColour(juce::TextButton::buttonOnColourId) : Theme::amber())
                   : (b.isColourSpecified(juce::TextButton::buttonColourId) ? b.findColour(juce::TextButton::buttonColourId) : Theme::control());
    if (! b.isEnabled())
        fill = fill.withMultipliedAlpha(0.45f);
    else if (down)
        fill = fill.darker(0.18f);
    else if (over)
        fill = fill.brighter(on ? 0.08f : 0.16f);
    g.setColour(fill);
    g.fillPath(shape);

    // A hairline outline; a faint top highlight gives depth without a bevel.
    g.setColour(on ? fill.brighter(0.25f).withAlpha(0.55f) : juce::Colours::white.withAlpha(over && b.isEnabled() ? 0.16f : 0.07f));
    g.strokePath(shape, juce::PathStrokeType(1.0f));
    if (! on && b.isEnabled() && ! down)
    {
        g.setColour(juce::Colours::white.withAlpha(0.035f));
        g.fillRect(bounds.reduced(radius, 1.0f).withHeight(1.0f));
    }
    // Keyboard focus ring.
    if (b.hasKeyboardFocus(false) && b.getWantsKeyboardFocus())
    {
        g.setColour(Theme::amberBright().withAlpha(0.6f));
        g.strokePath(controlShape(bounds.reduced(1.5f), radius - 1.0f, b.getConnectedEdgeFlags()), juce::PathStrokeType(1.4f));
    }
}

juce::Font StudioLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(std::clamp(static_cast<float>(buttonHeight) * 0.5f, 10.5f, 14.0f)));
}

void StudioLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);
    g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                    .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.4f));
    const auto inset = std::min(8, button.getWidth() / 6);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(inset, 1), juce::Justification::centred, 1, 0.8f);
}

void StudioLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
    float value, float start, float end, juce::Slider& slider)
{
    const auto size = juce::jmin(108.0f, float(juce::jmin(w, h)) - 8);
    auto r = juce::Rectangle<float>(size, size).withCentre({ x + w * .5f, y + h * .5f });
    const auto small = size < 40.0f;
    const auto ring = small ? 2.5f : 4.0f;
    juce::Path arc; arc.addCentredArc(r.getCentreX(), r.getCentreY(), size*.47f, size*.47f, 0, start, end, true);
    g.setColour(Theme::well()); g.strokePath(arc, juce::PathStrokeType(ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // Bipolar ranges (pan) fill from the centre.
    const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 && std::abs(slider.getMinimum() + slider.getMaximum()) < 1.0e-6;
    const auto from = bipolar ? (start + end) * 0.5f : start;
    const auto angle = start + value*(end-start);
    juce::Path active; active.addCentredArc(r.getCentreX(), r.getCentreY(), size*.47f, size*.47f, 0, std::min(from, angle), std::max(from, angle), true);
    g.setColour(slider.isEnabled() ? Theme::amber() : Theme::subduedText().withAlpha(0.4f));
    g.strokePath(active, juce::PathStrokeType(ring - 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    r.reduce(small ? 5.0f : 9.0f, small ? 5.0f : 9.0f);
    if (! small)
    {
        g.setColour(juce::Colours::black.withAlpha(.45f)); g.fillEllipse(r.translated(0, 3));
    }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5d5044), r.getX(), r.getY(),
        juce::Colour(0xff26211c), r.getRight(), r.getBottom(), false)); g.fillEllipse(r);
    g.setColour(juce::Colours::white.withAlpha(.14f)); g.drawEllipse(r, 1);
    auto c = r.getCentre();
    g.setColour(Theme::amberBright());
    const auto inner = small ? 0.0f : size * .20f;
    g.drawLine(c.x + std::sin(angle)*inner, c.y - std::cos(angle)*inner,
        c.x + std::sin(angle)*r.getWidth()*.45f, c.y - std::cos(angle)*r.getWidth()*.45f, small ? 1.6f : 3.0f);
}

int StudioLookAndFeel::getSliderThumbRadius(juce::Slider& slider)
{
    if (slider.getSliderStyle() == juce::Slider::LinearHorizontal || slider.getSliderStyle() == juce::Slider::LinearVertical)
        return 7;
    return LookAndFeel_V4::getSliderThumbRadius(slider);
}

void StudioLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float position, float minPosition, float maxPosition, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, position, minPosition, maxPosition, style, slider);
        return;
    }
    const auto horizontal = style == juce::Slider::LinearHorizontal;
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    const auto thickness = 4.0f;
    const auto track = horizontal ? area.withSizeKeepingCentre(area.getWidth(), thickness)
                                  : area.withSizeKeepingCentre(thickness, area.getHeight());
    g.setColour(Theme::well());
    g.fillRoundedRectangle(track.expanded(0.5f), thickness * 0.5f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(track.expanded(0.5f), thickness * 0.5f, 1.0f);

    const auto accent = slider.isEnabled() ? slider.findColour(juce::Slider::trackColourId, true) : Theme::subduedText().withAlpha(0.4f);
    // Bipolar ranges (EQ gain) fill from zero, the others from the minimum.
    const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 && std::abs(slider.getMinimum() + slider.getMaximum()) < 1.0e-6;
    auto filled = horizontal ? track.withRight(position) : track.withTop(position);
    if (bipolar)
    {
        const auto zero = static_cast<float>(slider.getPositionOfValue(0.0));
        filled = horizontal ? track.withLeft(std::min(zero, position)).withRight(std::max(zero, position))
                            : track.withTop(std::min(zero, position)).withBottom(std::max(zero, position));
    }
    g.setColour(accent);
    g.fillRoundedRectangle(filled, thickness * 0.5f);

    const auto centre = horizontal ? juce::Point<float>(position, track.getCentreY()) : juce::Point<float>(track.getCentreX(), position);
    const auto thumb = juce::Rectangle<float>(13.0f, 13.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(thumb.translated(0.0f, 1.0f));
    g.setColour(slider.isMouseOverOrDragging() ? juce::Colours::white : Theme::text());
    g.fillEllipse(thumb);
    g.setColour(accent);
    g.drawEllipse(thumb.reduced(0.5f), 1.4f);
}

void StudioLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
    const auto radius = std::min(6.0f, bounds.getHeight() * 0.5f);
    const auto over = box.isMouseOver(true) && box.isEnabled();
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId).brighter(over ? 0.08f : 0.0f).darker(down ? 0.15f : 0.0f));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(box.hasKeyboardFocus(true) || over ? Theme::amber().withAlpha(0.55f) : juce::Colours::white.withAlpha(0.09f));
    g.drawRoundedRectangle(bounds, radius, 1.0f);

    const auto arrow = juce::Rectangle<float>(static_cast<float>(width) - 20.0f, 0.0f, 12.0f, static_cast<float>(height));
    juce::Path chevron;
    chevron.startNewSubPath(arrow.getX() + 2.0f, arrow.getCentreY() - 2.0f);
    chevron.lineTo(arrow.getCentreX(), arrow.getCentreY() + 2.0f);
    chevron.lineTo(arrow.getRight() - 2.0f, arrow.getCentreY() - 2.0f);
    g.setColour((over ? Theme::amberBright() : Theme::subduedText()).withAlpha(box.isEnabled() ? 0.9f : 0.3f));
    g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font StudioLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::Font(juce::FontOptions(std::clamp(static_cast<float>(box.getHeight()) * 0.52f, 11.0f, 14.5f)));
}

void StudioLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    const auto inset = box.getHeight() < 24 ? 3 : 6;
    label.setBounds(inset, 1, box.getWidth() - inset - 22, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

void StudioLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRect(0, 0, width, height);
}

void StudioLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height,
    bool vertical, int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour(bar.findColour(juce::ScrollBar::trackColourId));
    g.fillRect(x, y, width, height);
    if (thumbSize <= 0)
        return;
    auto thumb = vertical ? juce::Rectangle<int>(x, thumbStart, width, thumbSize) : juce::Rectangle<int>(thumbStart, y, thumbSize, height);
    const auto inset = over || down ? 2.0f : 3.0f;
    const auto r = thumb.toFloat().reduced(inset);
    auto colour = bar.findColour(juce::ScrollBar::thumbColourId);
    g.setColour(down ? colour.brighter(0.3f) : over ? colour.brighter(0.15f) : colour);
    g.fillRoundedRectangle(r, std::min(r.getWidth(), r.getHeight()) * 0.5f);
}

juce::Rectangle<int> StudioLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    juce::AttributedString text;
    text.append(tipText, juce::Font(juce::FontOptions(13.0f)), Theme::text());
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(text, 420.0f);
    const auto w = static_cast<int>(layout.getWidth() + 22.0f);
    const auto h = static_cast<int>(layout.getHeight() + 14.0f);
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

void StudioLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& tipText, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height).toFloat();
    g.setColour(juce::Colour(0xff26201b));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(Theme::amber().withAlpha(0.35f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
    juce::AttributedString text;
    text.append(tipText, juce::Font(juce::FontOptions(13.0f)), Theme::text());
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(text, 420.0f);
    layout.draw(g, bounds.reduced(11.0f, 7.0f));
}

void StudioLookAndFeel::drawTickBox(juce::Graphics& g, juce::Component& component, float x, float y, float w, float h,
    bool ticked, bool enabled, bool over, bool)
{
    const auto box = juce::Rectangle<float>(x, y, w, h).withSizeKeepingCentre(std::min(w, h) * 0.85f, std::min(w, h) * 0.85f);
    g.setColour(ticked ? Theme::amber() : Theme::well());
    g.fillRoundedRectangle(box, 4.0f);
    g.setColour(ticked ? Theme::amber().brighter(0.2f) : juce::Colours::white.withAlpha(over ? 0.35f : 0.2f));
    g.drawRoundedRectangle(box.reduced(0.5f), 4.0f, 1.0f);
    if (ticked)
    {
        juce::Path tick;
        tick.startNewSubPath(box.getX() + box.getWidth() * 0.25f, box.getCentreY());
        tick.lineTo(box.getX() + box.getWidth() * 0.43f, box.getY() + box.getHeight() * 0.68f);
        tick.lineTo(box.getRight() - box.getWidth() * 0.24f, box.getY() + box.getHeight() * 0.3f);
        g.setColour(Theme::onText().withAlpha(enabled ? 1.0f : 0.5f));
        g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ignoreUnused(component);
}

void StudioLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(juce::Rectangle<int>(width, height).toFloat(), 5.0f);
}

void StudioLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.isEnabled())
        return;
    const auto focused = editor.hasKeyboardFocus(true) && ! editor.isReadOnly();
    g.setColour(focused ? Theme::amber().withAlpha(0.7f) : juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(juce::Rectangle<int>(width, height).toFloat().reduced(0.5f), 5.0f, focused ? 1.5f : 1.0f);
}

void AmberTheme::applyTo(juce::LookAndFeel_V4& lookAndFeel)
{
    lookAndFeel.setColour(juce::ResizableWindow::backgroundColourId, background());
    lookAndFeel.setColour(juce::Label::textColourId, text());
    lookAndFeel.setColour(juce::TextButton::buttonColourId, control());
    lookAndFeel.setColour(juce::TextButton::buttonOnColourId, amber());
    lookAndFeel.setColour(juce::TextButton::textColourOffId, text());
    lookAndFeel.setColour(juce::TextButton::textColourOnId, onText());
    lookAndFeel.setColour(juce::ToggleButton::textColourId, text());
    lookAndFeel.setColour(juce::ToggleButton::tickColourId, amber());
    lookAndFeel.setColour(juce::Slider::thumbColourId, amber());
    lookAndFeel.setColour(juce::Slider::trackColourId, amber());
    lookAndFeel.setColour(juce::Slider::backgroundColourId, well());
    lookAndFeel.setColour(juce::Slider::rotarySliderFillColourId, amber());
    lookAndFeel.setColour(juce::Slider::rotarySliderOutlineColourId, surfaceRaised());
    lookAndFeel.setColour(juce::Slider::textBoxTextColourId, text());
    lookAndFeel.setColour(juce::Slider::textBoxBackgroundColourId, background());
    lookAndFeel.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lookAndFeel.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff231d18));
    lookAndFeel.setColour(juce::ComboBox::textColourId, text());
    lookAndFeel.setColour(juce::ComboBox::outlineColourId, outline());
    lookAndFeel.setColour(juce::ComboBox::arrowColourId, subduedText());
    lookAndFeel.setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff221c17));
    lookAndFeel.setColour(juce::PopupMenu::textColourId, text());
    lookAndFeel.setColour(juce::PopupMenu::headerTextColourId, subduedText());
    lookAndFeel.setColour(juce::PopupMenu::highlightedBackgroundColourId, amber().withAlpha(0.22f));
    lookAndFeel.setColour(juce::PopupMenu::highlightedTextColourId, amberBright());
    lookAndFeel.setColour(juce::ScrollBar::thumbColourId, juce::Colours::white.withAlpha(0.22f));
    lookAndFeel.setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    lookAndFeel.setColour(juce::TextEditor::backgroundColourId, well());
    lookAndFeel.setColour(juce::TextEditor::textColourId, text());
    lookAndFeel.setColour(juce::TextEditor::highlightColourId, amber().withAlpha(0.35f));
    lookAndFeel.setColour(juce::TextEditor::outlineColourId, outline());
    lookAndFeel.setColour(juce::TextEditor::focusedOutlineColourId, amber());
    lookAndFeel.setColour(juce::CaretComponent::caretColourId, amberBright());
    lookAndFeel.setColour(juce::TooltipWindow::textColourId, text());
}
} // namespace nam::ui
