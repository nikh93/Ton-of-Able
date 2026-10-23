#include "SignalChainStrip.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using Theme = AmberTheme;
using daw::text;

namespace
{
constexpr float gap = 10.0f; // room for the connector arrow between blocks
}

void SignalChainStrip::setBlocks(std::vector<Block> next)
{
    const auto changed = next.size() != items.size()
        || ! std::equal(next.begin(), next.end(), items.begin(), [](const Block& a, const Block& b)
           { return a.id == b.id && a.label == b.label && a.detail == b.detail && a.active == b.active && a.empty == b.empty; });
    items = std::move(next);
    if (changed)
        repaint();
}

void SignalChainStrip::setSelected(const juce::String& id)
{
    if (selected != id)
    {
        selected = id;
        repaint();
    }
}

juce::Rectangle<float> SignalChainStrip::blockBounds(size_t index) const
{
    const auto count = static_cast<float>(std::max<size_t>(1, items.size()));
    const auto width = (static_cast<float>(getWidth()) - gap * (count - 1.0f)) / count;
    return { static_cast<float>(index) * (width + gap), 0.0f, width, static_cast<float>(getHeight()) };
}

juce::Rectangle<float> SignalChainStrip::ledBounds(size_t index) const
{
    // A generous hit area around the LED in the top-left corner.
    return blockBounds(index).removeFromTop(24.0f).removeFromLeft(24.0f);
}

juce::Rectangle<int> SignalChainStrip::screenBoundsOf(const juce::String& id) const
{
    for (size_t index = 0; index < items.size(); ++index)
        if (items[index].id == id)
            return localAreaToGlobal(blockBounds(index).toNearestInt());
    return getScreenBounds();
}

int SignalChainStrip::blockAt(juce::Point<float> point) const
{
    for (size_t index = 0; index < items.size(); ++index)
        if (blockBounds(index).contains(point))
            return static_cast<int>(index);
    return -1;
}

void SignalChainStrip::paint(juce::Graphics& g)
{
    for (size_t index = 0; index < items.size(); ++index)
    {
        const auto& block = items[index];
        auto area = blockBounds(index);
        const auto isHovered = static_cast<int>(index) == hovered;
        const auto isSelected = block.id == selected;

        // Connector to the next block: the signal flows left to right.
        if (index + 1 < items.size())
        {
            const auto y = area.getCentreY();
            const auto x = area.getRight();
            g.setColour(Theme::amber().withAlpha(0.45f));
            g.drawLine(x + 1.0f, y, x + gap - 2.0f, y, 1.5f);
            juce::Path arrow;
            arrow.addTriangle(x + gap - 1.0f, y, x + gap - 5.0f, y - 3.0f, x + gap - 5.0f, y + 3.0f);
            g.fillPath(arrow);
        }

        auto body = area.reduced(0.5f);
        const auto fill = block.active ? juce::Colour(0xff3d2f22) : juce::Colour(0xff241e19);
        g.setColour(isHovered ? fill.brighter(0.12f) : fill);
        if (block.empty)
        {
            g.setColour(juce::Colour(0xff1c1815));
            g.fillRoundedRectangle(body, 7.0f);
            g.setColour(Theme::subduedText().withAlpha(0.35f));
            const float dashes[] { 4.0f, 3.0f };
            juce::Path outline;
            outline.addRoundedRectangle(body, 7.0f);
            juce::Path dashed;
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
            g.fillPath(dashed);
        }
        else
        {
            g.fillRoundedRectangle(body, 7.0f);
            g.setColour(block.active ? Theme::amber().withAlpha(0.65f) : juce::Colours::white.withAlpha(0.12f));
            g.drawRoundedRectangle(body, 7.0f, block.active ? 1.4f : 1.0f);
        }
        if (isSelected)
        {
            g.setColour(Theme::amberBright());
            g.drawRoundedRectangle(body.reduced(1.5f), 6.0f, 2.0f);
        }

        // LED: lit when the block processes.
        if (block.toggleable)
        {
            const auto led = ledBounds(index).withSizeKeepingCentre(10.0f, 10.0f).translated(2.0f, 2.0f);
            if (block.active)
            {
                g.setColour(Theme::amberBright().withAlpha(0.35f));
                g.fillEllipse(led.expanded(3.0f));
            }
            g.setColour(block.active ? Theme::amberBright() : juce::Colour(0xff4a3f35));
            g.fillEllipse(led);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawEllipse(led, 1.0f);
            if (isHovered && hoveredLed)
            {
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.drawEllipse(led.expanded(2.5f), 1.0f);
            }
        }

        auto content = area.reduced(6.0f, 4.0f);
        content.removeFromTop(block.toggleable ? 14.0f : 6.0f);
        g.setColour(block.active ? Theme::text() : Theme::subduedText());
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawFittedText(block.label, content.removeFromTop(16.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
        g.setColour(block.active ? Theme::amberBright().withAlpha(0.9f) : Theme::subduedText().withAlpha(0.75f));
        g.setFont(juce::FontOptions(10.5f));
        g.drawFittedText(block.detail, content.toNearestInt(), juce::Justification::centredTop, 2, 0.75f);
    }
}

void SignalChainStrip::mouseMove(const juce::MouseEvent& event)
{
    const auto index = blockAt(event.position);
    const auto onLed = index >= 0 && items[static_cast<size_t>(index)].toggleable && ledBounds(static_cast<size_t>(index)).contains(event.position);
    if (index != hovered || onLed != hoveredLed)
    {
        hovered = index;
        hoveredLed = onLed;
        setMouseCursor(index >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SignalChainStrip::mouseExit(const juce::MouseEvent&)
{
    hovered = -1;
    hoveredLed = false;
    repaint();
}

void SignalChainStrip::mouseUp(const juce::MouseEvent& event)
{
    if (! event.mouseWasClicked())
        return;
    const auto index = blockAt(event.position);
    if (index < 0)
        return;
    const auto& block = items[static_cast<size_t>(index)];
    const auto id = block.id; // the callbacks may replace the blocks
    if (block.toggleable && ! block.empty && ledBounds(static_cast<size_t>(index)).contains(event.position))
    {
        if (onToggle)
            onToggle(id);
    }
    else if (onOpen)
        onOpen(id);
}

juce::String SignalChainStrip::getTooltip()
{
    const auto index = blockAt(getMouseXYRelative().toFloat());
    if (index < 0)
        return {};
    const auto& block = items[static_cast<size_t>(index)];
    auto tip = block.tooltip;
    if (block.toggleable && ! block.empty)
        tip << (tip.isEmpty() ? "" : "\n") << text("Punkt: an/aus · Block: bearbeiten");
    else
        tip << (tip.isEmpty() ? "" : "\n") << text("Klick: bearbeiten");
    return tip;
}
} // namespace nam::ui
