#include "Icons.h"

namespace nam::ui
{
namespace
{
/** Paths are designed on a 24 x 24 grid. */
struct IconPaths
{
    juce::Path fill, stroke;
};

IconPaths makePaths(Icon icon)
{
    IconPaths p;
    auto& f = p.fill;
    auto& s = p.stroke;
    switch (icon)
    {
        case Icon::none: break;
        case Icon::play: f.addTriangle(7.0f, 4.5f, 7.0f, 19.5f, 19.5f, 12.0f); break;
        case Icon::stop: f.addRoundedRectangle(6.0f, 6.0f, 12.0f, 12.0f, 1.5f); break;
        case Icon::record: f.addEllipse(5.5f, 5.5f, 13.0f, 13.0f); break;
        case Icon::toStart:
            f.addRoundedRectangle(5.0f, 5.5f, 2.6f, 13.0f, 0.8f);
            f.addTriangle(19.0f, 5.5f, 19.0f, 18.5f, 9.0f, 12.0f);
            break;
        case Icon::loop:
            s.startNewSubPath(5.0f, 13.0f);
            s.lineTo(5.0f, 10.0f);
            s.quadraticTo(5.0f, 7.0f, 8.0f, 7.0f);
            s.lineTo(18.0f, 7.0f);
            s.startNewSubPath(19.0f, 11.0f);
            s.lineTo(19.0f, 14.0f);
            s.quadraticTo(19.0f, 17.0f, 16.0f, 17.0f);
            s.lineTo(6.0f, 17.0f);
            f.addTriangle(16.0f, 3.8f, 16.0f, 10.2f, 20.5f, 7.0f);
            f.addTriangle(8.0f, 13.8f, 8.0f, 20.2f, 3.5f, 17.0f);
            break;
        case Icon::metronome:
            s.startNewSubPath(9.5f, 4.0f);
            s.lineTo(14.5f, 4.0f);
            s.lineTo(18.5f, 20.0f);
            s.lineTo(5.5f, 20.0f);
            s.closeSubPath();
            s.startNewSubPath(12.0f, 16.0f);
            s.lineTo(17.5f, 7.0f);
            f.addEllipse(10.4f, 14.4f, 3.2f, 3.2f);
            break;
        case Icon::countIn:
            // "1-2-3-4" ticks with a count arrow: four bars of rising height.
            for (int index = 0; index < 4; ++index)
            {
                const auto height = 5.0f + static_cast<float>(index) * 3.5f;
                f.addRoundedRectangle(4.0f + static_cast<float>(index) * 4.5f, 20.0f - height, 2.8f, height, 1.0f);
            }
            break;
        case Icon::pointer:
            f.startNewSubPath(6.0f, 3.5f);
            f.lineTo(6.0f, 19.0f);
            f.lineTo(10.0f, 15.2f);
            f.lineTo(12.8f, 21.0f);
            f.lineTo(15.4f, 19.8f);
            f.lineTo(12.6f, 14.2f);
            f.lineTo(18.0f, 14.0f);
            f.closeSubPath();
            break;
        case Icon::scissors:
            s.addEllipse(4.0f, 14.0f, 6.0f, 6.0f);
            s.addEllipse(14.0f, 14.0f, 6.0f, 6.0f);
            s.startNewSubPath(9.0f, 15.0f);
            s.lineTo(17.5f, 3.5f);
            s.startNewSubPath(15.0f, 15.0f);
            s.lineTo(6.5f, 3.5f);
            break;
        case Icon::stretch:
            s.startNewSubPath(4.0f, 12.0f);
            s.lineTo(20.0f, 12.0f);
            s.startNewSubPath(4.0f, 5.0f);
            s.lineTo(4.0f, 19.0f);
            s.startNewSubPath(20.0f, 5.0f);
            s.lineTo(20.0f, 19.0f);
            f.addTriangle(5.5f, 12.0f, 9.5f, 9.0f, 9.5f, 15.0f);
            f.addTriangle(18.5f, 12.0f, 14.5f, 9.0f, 14.5f, 15.0f);
            break;
        case Icon::zoomIn:
        case Icon::zoomOut:
            s.addEllipse(4.0f, 4.0f, 12.0f, 12.0f);
            s.startNewSubPath(14.5f, 14.5f);
            s.lineTo(20.0f, 20.0f);
            s.startNewSubPath(7.0f, 10.0f);
            s.lineTo(13.0f, 10.0f);
            if (icon == Icon::zoomIn)
            {
                s.startNewSubPath(10.0f, 7.0f);
                s.lineTo(10.0f, 13.0f);
            }
            break;
        case Icon::zoomFit:
            for (const auto& corner : { juce::Point<float>(4.0f, 4.0f), { 20.0f, 4.0f }, { 4.0f, 20.0f }, { 20.0f, 20.0f } })
            {
                const auto dx = corner.x < 12.0f ? 5.0f : -5.0f;
                const auto dy = corner.y < 12.0f ? 5.0f : -5.0f;
                s.startNewSubPath(corner.x + dx, corner.y);
                s.lineTo(corner);
                s.lineTo(corner.x, corner.y + dy);
            }
            f.addRoundedRectangle(8.5f, 9.5f, 7.0f, 5.0f, 1.0f);
            break;
        case Icon::undo:
        case Icon::redo:
        {
            juce::Path arrow;
            arrow.startNewSubPath(8.0f, 9.0f);
            arrow.lineTo(14.0f, 9.0f);
            arrow.cubicTo(17.5f, 9.0f, 19.5f, 11.5f, 19.5f, 14.0f);
            arrow.cubicTo(19.5f, 16.5f, 17.5f, 19.0f, 14.0f, 19.0f);
            arrow.lineTo(10.0f, 19.0f);
            juce::Path head;
            head.addTriangle(4.0f, 9.0f, 9.0f, 4.8f, 9.0f, 13.2f);
            if (icon == Icon::redo)
            {
                const auto mirror = juce::AffineTransform::scale(-1.0f, 1.0f).translated(24.0f, 0.0f);
                arrow.applyTransform(mirror);
                head.applyTransform(mirror);
            }
            s = arrow;
            f = head;
            break;
        }
        case Icon::plus:
            s.startNewSubPath(12.0f, 5.0f);
            s.lineTo(12.0f, 19.0f);
            s.startNewSubPath(5.0f, 12.0f);
            s.lineTo(19.0f, 12.0f);
            break;
        case Icon::importFile:
        case Icon::exportFile:
            s.startNewSubPath(4.5f, 14.0f);
            s.lineTo(4.5f, 19.5f);
            s.lineTo(19.5f, 19.5f);
            s.lineTo(19.5f, 14.0f);
            s.startNewSubPath(12.0f, 3.5f);
            s.lineTo(12.0f, 14.0f);
            if (icon == Icon::importFile)
                f.addTriangle(7.5f, 11.0f, 16.5f, 11.0f, 12.0f, 16.0f);
            else
                f.addTriangle(7.5f, 7.0f, 16.5f, 7.0f, 12.0f, 2.0f);
            break;
        case Icon::folder:
            s.startNewSubPath(3.5f, 7.0f);
            s.lineTo(3.5f, 18.5f);
            s.lineTo(20.5f, 18.5f);
            s.lineTo(20.5f, 8.5f);
            s.lineTo(11.5f, 8.5f);
            s.lineTo(9.5f, 5.5f);
            s.lineTo(3.5f, 5.5f);
            s.closeSubPath();
            break;
        case Icon::keyboard:
            s.addRoundedRectangle(2.5f, 6.0f, 19.0f, 12.0f, 2.0f);
            for (int index = 0; index < 4; ++index)
            {
                f.addRectangle(5.0f + static_cast<float>(index) * 4.0f, 8.8f, 2.0f, 2.0f);
                f.addRectangle(5.0f + static_cast<float>(index) * 4.0f, 11.6f, 2.0f, 2.0f);
            }
            f.addRectangle(7.0f, 14.5f, 10.0f, 1.8f);
            break;
        case Icon::snowflake:
            for (int index = 0; index < 3; ++index)
            {
                const auto angle = juce::MathConstants<float>::pi * static_cast<float>(index) / 3.0f;
                const auto dx = std::sin(angle) * 8.5f, dy = std::cos(angle) * 8.5f;
                s.startNewSubPath(12.0f - dx, 12.0f - dy);
                s.lineTo(12.0f + dx, 12.0f + dy);
            }
            break;
        case Icon::headphones:
            s.startNewSubPath(5.0f, 15.0f);
            s.lineTo(5.0f, 12.0f);
            s.cubicTo(5.0f, 3.0f, 19.0f, 3.0f, 19.0f, 12.0f);
            s.lineTo(19.0f, 15.0f);
            f.addRoundedRectangle(3.5f, 13.0f, 4.5f, 7.5f, 1.5f);
            f.addRoundedRectangle(16.0f, 13.0f, 4.5f, 7.5f, 1.5f);
            break;
        case Icon::sliders:
            for (int index = 0; index < 3; ++index)
            {
                const auto x = 6.0f + static_cast<float>(index) * 6.0f;
                s.startNewSubPath(x, 4.0f);
                s.lineTo(x, 20.0f);
                const auto knob = index == 1 ? 7.0f : (index == 0 ? 13.0f : 10.0f);
                f.addRoundedRectangle(x - 2.5f, knob, 5.0f, 4.0f, 1.0f);
            }
            break;
    }
    return p;
}
} // namespace

void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour)
{
    if (icon == Icon::none)
        return;
    const auto size = std::min(area.getWidth(), area.getHeight());
    const auto scale = size / 24.0f;
    const auto transform = juce::AffineTransform::scale(scale).translated(area.getCentreX() - size * 0.5f, area.getCentreY() - size * 0.5f);
    auto paths = makePaths(icon);
    g.setColour(colour);
    if (! paths.fill.isEmpty())
        g.fillPath(paths.fill, transform);
    if (! paths.stroke.isEmpty())
        g.strokePath(paths.stroke, juce::PathStrokeType(std::max(1.2f, 2.0f * scale), juce::PathStrokeType::curved, juce::PathStrokeType::rounded), transform);
}

IconButton::IconButton(Icon icon, const juce::String& label) : juce::TextButton(label), shape(icon) {}

void IconButton::setIcon(Icon icon)
{
    if (shape != icon)
    {
        shape = icon;
        repaint();
    }
}

void IconButton::setIconColour(juce::Colour colour)
{
    iconColour = colour;
    repaint();
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto& lf = getLookAndFeel();
    const auto background = findColour(getToggleState() ? buttonOnColourId : buttonColourId);
    lf.drawButtonBackground(g, *this, background, highlighted, down);

    const auto on = getToggleState();
    auto textColour = findColour(on ? textColourOnId : textColourOffId);
    auto glyph = on || ! iconColour.has_value() ? textColour : *iconColour;
    if (! isEnabled())
    {
        textColour = textColour.withMultipliedAlpha(0.4f);
        glyph = glyph.withMultipliedAlpha(0.4f);
    }

    auto area = getLocalBounds().toFloat().reduced(4.0f, 3.0f);
    const auto iconSize = std::min(18.0f, area.getHeight());
    const auto label = getButtonText();
    if (label.isEmpty())
    {
        drawIcon(g, shape, area.withSizeKeepingCentre(iconSize, iconSize), glyph);
        return;
    }
    const juce::Font font(juce::FontOptions(13.0f));
    const auto textWidth = juce::GlyphArrangement::getStringWidth(font, label);
    const auto hasIcon = shape != Icon::none;
    const auto total = (hasIcon ? iconSize + 6.0f : 0.0f) + textWidth;
    auto content = area.withSizeKeepingCentre(std::min(total, area.getWidth()), area.getHeight());
    if (hasIcon)
    {
        drawIcon(g, shape, content.removeFromLeft(iconSize).withSizeKeepingCentre(iconSize, iconSize), glyph);
        content.removeFromLeft(6.0f);
    }
    g.setColour(textColour);
    g.setFont(font);
    g.drawFittedText(label, content.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
}
} // namespace nam::ui
