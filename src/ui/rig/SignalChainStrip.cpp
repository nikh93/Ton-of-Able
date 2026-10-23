#include "SignalChainStrip.h"

#include <algorithm>

#include <map>

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using Theme = AmberTheme;
using daw::text;

namespace
{
constexpr float gap = 10.0f;        // room for the connector arrow between blocks
constexpr float rowHeight = 66.0f;
constexpr float band = 22.0f;       // between the rows: where the connection curves run
constexpr float terminalWidth = 40.0f;
constexpr float addWidth = 30.0f;
constexpr float minBlockWidth = 70.0f;
const juce::Colour midiColour { 0xff5aa9e6 };

/** The levels of the connections of one block (or all), editable in place. */
class ConnectionsPanel final : public juce::Component
{
public:
    ConnectionsPanel(const chain::Layout& layout, juce::String focus, std::function<juce::String(const juce::String&)> name,
                     std::function<void(const chain::Layout&)> changed)
        : working(layout), id(std::move(focus)), nameOf(std::move(name)), onChange(std::move(changed))
    {
        title.setText(id.isEmpty() ? text("Alle Verbindungen") : text("Verbindungen von ") + nameOf(id), juce::dontSendNotification);
        title.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, Theme::text());
        addAndMakeVisible(title);
        hint.setColour(juce::Label::textColourId, Theme::subduedText());
        hint.setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(hint);
        rebuild();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 10);
        title.setBounds(area.removeFromTop(22));
        area.removeFromTop(6);
        for (auto& row : rows)
        {
            auto line = area.removeFromTop(30);
            row->remove.setBounds(line.removeFromRight(28).reduced(2));
            line.removeFromRight(6);
            row->label.setBounds(line.removeFromLeft(250));
            row->level.setBounds(line);
            area.removeFromTop(2);
        }
        hint.setBounds(area.removeFromTop(36));
    }

private:
    struct Row
    {
        chain::Link link;
        juce::Label label;
        juce::Slider level { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::TextButton remove { juce::String(juce::CharPointer_UTF8("\xc3\x97")) };
    };

    void rebuild()
    {
        rows.clear();
        for (const auto& link : working.links)
        {
            if (id.isNotEmpty() && link.from != id && link.to != id)
                continue;
            auto row = std::make_unique<Row>();
            row->link = link;
            row->label.setText(nameOf(link.from) + text(" → ") + nameOf(link.to) + (link.midi ? text("  · MIDI") : juce::String()),
                               juce::dontSendNotification);
            row->label.setColour(juce::Label::textColourId, link.midi ? midiColour : Theme::text());
            row->level.setRange(-60.0, 12.0, 0.1);
            row->level.setSkewFactorFromMidPoint(-12.0);
            row->level.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 20);
            row->level.textFromValueFunction = [](double v) { return v <= -59.9 ? juce::String("aus") : juce::String(v, 1) + " dB"; };
            row->level.setValue(link.gainDb, juce::dontSendNotification);
            row->level.updateText();
            row->level.setDoubleClickReturnValue(true, 0.0);
            row->level.onValueChange = [this, raw = row.get()]
            {
                auto changed = raw->link;
                changed.gainDb = static_cast<float>(raw->level.getValue());
                working.addLink(changed);
                raw->link = changed;
                onChange(working);
            };
            row->remove.setTooltip(text("Verbindung entfernen"));
            row->remove.onClick = [this, link]
            {
                working.removeLink(link);
                onChange(working);
                juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ConnectionsPanel>(this)]
                {
                    if (safe != nullptr)
                        safe->rebuild();
                });
            };
            addAndMakeVisible(row->label);
            if (! link.midi) // MIDI connections carry no level
                addAndMakeVisible(row->level);
            addAndMakeVisible(row->remove);
            rows.push_back(std::move(row));
        }
        hint.setText(rows.empty() ? text("Keine Verbindungen. Rechtsklick auf einen Block: „Audio senden an“ / „MIDI senden an“.")
                                  : text("Mehrere Verbindungen in einen Eingang werden addiert. Doppelklick auf einen Regler: 0 dB."),
                     juce::dontSendNotification);
        setSize(560, 70 + static_cast<int>(rows.size()) * 32 + 30);
        resized();
    }

    chain::Layout working;
    juce::String id;
    std::function<juce::String(const juce::String&)> nameOf;
    std::function<void(const chain::Layout&)> onChange;
    juce::Label title, hint;
    std::vector<std::unique_ptr<Row>> rows;
};
} // namespace

void SignalChainStrip::setLayout(const chain::Layout& layout)
{
    current = layout;
    refresh();
}

void SignalChainStrip::refresh()
{
    described.clear();
    if (describe)
        for (const auto* list : { &current.serial, &current.side })
            for (const auto& node : *list)
                described[node.id] = describe(node);
    layoutItems();
    repaint();
}

bool SignalChainStrip::hasSideRow() const
{
    return ! current.side.empty();
}

int SignalChainStrip::preferredHeight() const
{
    return static_cast<int>(rowHeight + band + (hasSideRow() ? rowHeight * 0.85f : 0.0f));
}

int SignalChainStrip::preferredWidth(int available) const
{
    const auto count = static_cast<float>(current.serial.size());
    const auto needed = 2.0f * terminalWidth + addWidth + count * minBlockWidth + (count + 2.0f) * gap;
    return std::max(available, static_cast<int>(std::ceil(needed)));
}

void SignalChainStrip::resized()
{
    layoutItems();
}

void SignalChainStrip::layoutItems()
{
    items.clear();
    const auto width = static_cast<float>(getWidth());
    const auto count = static_cast<float>(std::max<size_t>(1, current.serial.size()));
    const auto blockWidth = std::max(minBlockWidth, (width - 2.0f * terminalWidth - addWidth - (count + 2.0f) * gap) / count);
    float x = 0.0f;
    items.push_back({ ItemType::input, chain::input, { x, 0.0f, terminalWidth, rowHeight } });
    x += terminalWidth + gap;
    for (const auto& node : current.serial)
    {
        items.push_back({ ItemType::block, node.id, { x, 0.0f, blockWidth, rowHeight } });
        x += blockWidth + gap;
    }
    items.push_back({ ItemType::addSerial, {}, { x, rowHeight * 0.2f, addWidth, rowHeight * 0.6f } });
    items.push_back({ ItemType::output, chain::output, { width - terminalWidth, 0.0f, terminalWidth, rowHeight } });

    if (hasSideRow())
    {
        const auto top = rowHeight + band;
        const auto height = rowHeight * 0.85f;
        float sx = 0.0f;
        if (offerKeyboards)
        {
            items.push_back({ ItemType::keys, chain::keys, { sx, top, terminalWidth + 14.0f, height }, true });
            sx += terminalWidth + 14.0f + gap;
        }
        const auto sideWidth = std::min(blockWidth * 1.6f, 170.0f);
        for (const auto& node : current.side)
        {
            items.push_back({ ItemType::block, node.id, { sx, top, sideWidth, height }, true });
            sx += sideWidth + gap;
        }
        items.push_back({ ItemType::addSide, {}, { sx, top + height * 0.2f, addWidth, height * 0.6f }, true });
    }
}

const SignalChainStrip::Item* SignalChainStrip::findItem(const juce::String& id) const
{
    for (const auto& item : items)
        if (item.id == id && (item.type == ItemType::block || item.type == ItemType::input || item.type == ItemType::output || item.type == ItemType::keys))
            return &item;
    return nullptr;
}

int SignalChainStrip::itemAt(juce::Point<float> point) const
{
    for (size_t index = 0; index < items.size(); ++index)
        if (items[index].bounds.contains(point))
            return static_cast<int>(index);
    return -1;
}

juce::Rectangle<float> SignalChainStrip::ledBounds(const Item& item) const
{
    return item.bounds.withSize(24.0f, 24.0f); // generous hit area around the LED
}

juce::Rectangle<int> SignalChainStrip::screenBoundsOf(const juce::String& id) const
{
    if (const auto* item = findItem(id))
        return localAreaToGlobal(item->bounds.toNearestInt());
    return getScreenBounds();
}

void SignalChainStrip::setSelected(const juce::String& id)
{
    if (selected != id)
    {
        selected = id;
        repaint();
    }
}

juce::String SignalChainStrip::nameOf(const juce::String& id) const
{
    if (id == chain::input) return text("Gitarre");
    if (id == chain::keys) return text("MIDI-Keyboards");
    if (id == chain::output) return text("Ausgang");
    if (const auto it = described.find(id); it != described.end())
    {
        const auto* node = current.find(id);
        if (node != nullptr && ! node->isBuiltin() && it->second.detail.isNotEmpty())
            return it->second.detail;
        return it->second.label;
    }
    return id;
}

void SignalChainStrip::change(const chain::Layout& layout)
{
    current = layout;
    current.sanitise();
    refresh();
    if (onLayoutChanged)
        onLayoutChanged(current);
}

//==============================================================================
void SignalChainStrip::paintLinks(juce::Graphics& g)
{
    const auto anchor = [this](const Item& item, bool asSource) -> juce::Point<float>
    {
        if (item.sideRow)
            return { item.bounds.getCentreX() + (asSource ? 8.0f : -8.0f), item.bounds.getY() };
        // Serial row: curves leave and enter at the bottom edge, slightly apart.
        return { item.bounds.getCentreX() + (asSource ? 6.0f : -6.0f), item.bounds.getBottom() };
    };
    for (const auto& link : current.links)
    {
        const auto* from = findItem(link.from);
        const auto* to = findItem(link.to);
        if (from == nullptr || to == nullptr)
            continue;
        const auto start = anchor(*from, true), end = anchor(*to, false);
        juce::Path path;
        path.startNewSubPath(start);
        if (! from->sideRow && ! to->sideRow)
        {
            const auto depth = rowHeight + band * 0.85f;
            path.cubicTo(start.x, depth, end.x, depth, end.x, end.y);
        }
        else if (from->sideRow && to->sideRow)
        {
            // Within the lower row: an arch through the band above it.
            const auto height = start.y - band * 0.75f;
            path.cubicTo(start.x, height, end.x, height, end.x, end.y);
        }
        else
        {
            const auto middle = (start.y + end.y) * 0.5f;
            path.cubicTo(start.x, middle, end.x, middle, end.x, end.y);
        }
        const auto colour = link.midi ? midiColour : Theme::amber();
        const auto muted = ! link.midi && link.gainDb <= -59.9f;
        g.setColour(colour.withAlpha(muted ? 0.3f : 0.85f));
        if (link.midi)
        {
            juce::Path dashed;
            const float dashes[] { 5.0f, 3.0f };
            juce::PathStrokeType(1.8f).createDashedStroke(dashed, path, dashes, 2);
            g.fillPath(dashed);
        }
        else
            g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Arrow head pointing into the target.
        const auto pointsDown = to->sideRow && (end.y > start.y || from->sideRow);
        juce::Path arrow;
        if (pointsDown)
            arrow.addTriangle(end.x, end.y, end.x - 4.0f, end.y - 7.0f, end.x + 4.0f, end.y - 7.0f);
        else
            arrow.addTriangle(end.x, end.y, end.x - 4.0f, end.y + 7.0f, end.x + 4.0f, end.y + 7.0f);
        g.fillPath(arrow);
        if (! link.midi && link.gainDb != 0.0f)
        {
            const auto label = muted ? juce::String("aus") : (link.gainDb > 0 ? "+" : "") + juce::String(link.gainDb, 1) + " dB";
            const auto at = path.getPointAlongPath(path.getLength() * 0.5f);
            auto box = juce::Rectangle<float>(48.0f, 14.0f).withCentre(at);
            g.setColour(Theme::panel());
            g.fillRoundedRectangle(box, 4.0f);
            g.setColour(colour);
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(label, box, juce::Justification::centred);
        }
    }
}

void SignalChainStrip::paint(juce::Graphics& g)
{
    // Connector arrows along the serial row: the signal flows left to right.
    for (size_t index = 0; index + 1 < items.size(); ++index)
    {
        const auto& item = items[index];
        if (item.sideRow || item.type == ItemType::output || item.type == ItemType::addSerial)
            continue;
        const auto y = item.bounds.getCentreY();
        const auto x = item.bounds.getRight();
        g.setColour(Theme::amber().withAlpha(0.45f));
        g.drawLine(x + 1.0f, y, x + gap - 2.0f, y, 1.5f);
        juce::Path arrow;
        arrow.addTriangle(x + gap - 1.0f, y, x + gap - 5.0f, y - 3.0f, x + gap - 5.0f, y + 3.0f);
        g.fillPath(arrow);
    }
    if (hasSideRow())
    {
        g.setColour(Theme::subduedText().withAlpha(0.55f));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(text("NEBEN DER KETTE"), juce::Rectangle<float>(0.0f, rowHeight + 4.0f, 140.0f, 12.0f), juce::Justification::centredLeft);
    }

    for (size_t index = 0; index < items.size(); ++index)
    {
        const auto& item = items[index];
        const auto isHovered = static_cast<int>(index) == hovered;
        auto body = item.bounds.reduced(0.5f);
        if (item.type == ItemType::addSerial || item.type == ItemType::addSide)
        {
            g.setColour(isHovered ? Theme::amber().withAlpha(0.25f) : juce::Colours::white.withAlpha(0.05f));
            g.fillRoundedRectangle(body, 6.0f);
            g.setColour(isHovered ? Theme::amberBright() : Theme::subduedText());
            const auto c = body.getCentre();
            g.drawLine(c.x - 6.0f, c.y, c.x + 6.0f, c.y, 1.8f);
            g.drawLine(c.x, c.y - 6.0f, c.x, c.y + 6.0f, 1.8f);
            continue;
        }
        if (item.type != ItemType::block)
        {
            // Terminals: the guitar input, the output and the MIDI keyboards.
            g.setColour(Theme::well());
            g.fillRoundedRectangle(body, 7.0f);
            g.setColour(item.type == ItemType::keys ? midiColour.withAlpha(0.6f) : Theme::amber().withAlpha(0.5f));
            g.drawRoundedRectangle(body, 7.0f, 1.2f);
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(item.type == ItemType::input ? "IN" : item.type == ItemType::output ? "OUT" : "KEYS", body, juce::Justification::centred);
            if (item.type == ItemType::output && current.chainLevelDb != 0.0f)
            {
                g.setFont(juce::FontOptions(9.0f));
                g.drawText(current.chainLevelDb <= -59.9f ? "aus" : juce::String(current.chainLevelDb, 0) + " dB",
                           body.removeFromBottom(16.0f), juce::Justification::centred);
            }
            continue;
        }

        const auto it = described.find(item.id);
        const auto block = it != described.end() ? it->second : Block {};
        const auto isSelected = item.id == selected;
        const auto beingDragged = dragging && pressed == static_cast<int>(index);
        const auto fill = block.active ? juce::Colour(0xff3d2f22) : juce::Colour(0xff241e19);
        if (block.empty)
        {
            g.setColour(juce::Colour(0xff1c1815));
            g.fillRoundedRectangle(body, 7.0f);
            g.setColour(Theme::subduedText().withAlpha(0.35f));
            const float dashes[] { 4.0f, 3.0f };
            juce::Path outline, dashed;
            outline.addRoundedRectangle(body, 7.0f);
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
            g.fillPath(dashed);
        }
        else
        {
            g.setColour(isHovered ? fill.brighter(0.12f) : fill);
            g.fillRoundedRectangle(body, 7.0f);
            g.setColour(block.active ? Theme::amber().withAlpha(0.65f) : juce::Colours::white.withAlpha(0.12f));
            g.drawRoundedRectangle(body, 7.0f, block.active ? 1.4f : 1.0f);
        }
        if (beingDragged)
        {
            g.setColour(Theme::background().withAlpha(0.6f));
            g.fillRoundedRectangle(body, 7.0f);
        }
        if (isSelected)
        {
            g.setColour(Theme::amberBright());
            g.drawRoundedRectangle(body.reduced(1.5f), 6.0f, 2.0f);
        }
        if (block.toggleable)
        {
            const auto led = ledBounds(item).withSizeKeepingCentre(10.0f, 10.0f).translated(2.0f, 2.0f);
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
        auto content = item.bounds.reduced(6.0f, 4.0f);
        content.removeFromTop(block.toggleable ? 14.0f : 6.0f);
        g.setColour(block.active ? Theme::text() : Theme::subduedText());
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawFittedText(block.label, content.removeFromTop(16.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
        g.setColour(block.active ? Theme::amberBright().withAlpha(0.9f) : Theme::subduedText().withAlpha(0.75f));
        g.setFont(juce::FontOptions(10.5f));
        g.drawFittedText(block.detail, content.toNearestInt(), juce::Justification::centredTop, 2, 0.75f);
    }

    paintLinks(g);

    if (dragging && juce::isPositiveAndBelow(pressed, static_cast<int>(items.size())))
    {
        // Ghost of the dragged block and where it would land.
        bool toSide = false;
        const auto index = dropIndex(dragPosition, toSide);
        g.setColour(Theme::amberBright());
        if (toSide)
        {
            const auto top = rowHeight + band;
            g.drawRoundedRectangle(juce::Rectangle<float>(0.0f, top, static_cast<float>(getWidth()), rowHeight * 0.85f).reduced(1.0f), 7.0f, 1.5f);
        }
        else
        {
            float x = items.front().bounds.getRight() + gap * 0.5f;
            int serialCount = 0;
            for (const auto& item : items)
                if (item.type == ItemType::block && ! item.sideRow)
                {
                    if (serialCount == index)
                        break;
                    x = item.bounds.getRight() + gap * 0.5f;
                    ++serialCount;
                }
            g.fillRoundedRectangle(x - 2.0f, 2.0f, 4.0f, rowHeight - 4.0f, 2.0f);
        }
        const auto& item = items[static_cast<size_t>(pressed)];
        auto ghost = item.bounds.withCentre(dragPosition);
        g.setColour(Theme::amber().withAlpha(0.25f));
        g.fillRoundedRectangle(ghost, 7.0f);
        g.setColour(Theme::amberBright());
        g.drawRoundedRectangle(ghost, 7.0f, 1.5f);
    }
}

//==============================================================================
int SignalChainStrip::dropIndex(juce::Point<float> point, bool& toSide) const
{
    const auto& item = items[static_cast<size_t>(pressed)];
    const auto* node = current.find(item.id);
    toSide = point.y > rowHeight + band * 0.5f && node != nullptr && ! node->isBuiltin();
    int index = 0;
    for (const auto& other : items)
        if (other.type == ItemType::block && ! other.sideRow)
        {
            if (point.x < other.bounds.getCentreX())
                break;
            ++index;
        }
    return index;
}

void SignalChainStrip::mouseMove(const juce::MouseEvent& event)
{
    const auto index = itemAt(event.position);
    auto onLed = false;
    if (index >= 0 && items[static_cast<size_t>(index)].type == ItemType::block)
    {
        const auto it = described.find(items[static_cast<size_t>(index)].id);
        onLed = it != described.end() && it->second.toggleable && ledBounds(items[static_cast<size_t>(index)]).contains(event.position);
    }
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

void SignalChainStrip::mouseDown(const juce::MouseEvent& event)
{
    pressed = itemAt(event.position);
    dragging = false;
    if (event.mods.isPopupMenu())
    {
        const auto index = pressed;
        pressed = -1;
        if (index < 0)
        {
            showConnections({});
            return;
        }
        const auto& item = items[static_cast<size_t>(index)];
        if (item.type == ItemType::block || item.type == ItemType::input || item.type == ItemType::keys || item.type == ItemType::output)
            showBlockMenu(item.id);
    }
}

void SignalChainStrip::mouseDrag(const juce::MouseEvent& event)
{
    if (! juce::isPositiveAndBelow(pressed, static_cast<int>(items.size())) || items[static_cast<size_t>(pressed)].type != ItemType::block)
        return;
    if (! dragging && event.getDistanceFromDragStart() > 6)
        dragging = true;
    if (dragging)
    {
        dragPosition = event.position;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        repaint();
    }
}

void SignalChainStrip::mouseUp(const juce::MouseEvent& event)
{
    const auto index = pressed;
    pressed = -1;
    if (! juce::isPositiveAndBelow(index, static_cast<int>(items.size())) || event.mods.isPopupMenu())
        return;
    const auto item = items[static_cast<size_t>(index)];
    if (dragging)
    {
        dragging = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        auto layout = current;
        pressed = index; // dropIndex reads the dragged item
        bool toSide = false;
        const auto target = dropIndex(event.position, toSide);
        pressed = -1;
        if (toSide)
            layout.moveToSide(item.id);
        else
        {
            // Insert in front of the target block (positions counted without the dragged block).
            int position = target;
            if (const auto from = layout.serialIndex(item.id); from >= 0 && from < target)
                --position;
            layout.moveTo(item.id, position);
        }
        if (layout != current)
            change(layout);
        else
            repaint();
        return;
    }
    if (! event.mouseWasClicked())
        return;
    switch (item.type)
    {
        case ItemType::addSerial: showAddMenu(false); return;
        case ItemType::addSide: showAddMenu(true); return;
        case ItemType::input:
        case ItemType::output:
        case ItemType::keys: showBlockMenu(item.id); return;
        case ItemType::block: break;
    }
    const auto it = described.find(item.id);
    const auto id = item.id; // callbacks may rebuild the items
    if (it != described.end() && it->second.toggleable && ! it->second.empty && ledBounds(item).contains(event.position))
    {
        if (onToggle)
            onToggle(id);
    }
    else if (onOpen)
        onOpen(id);
}

//==============================================================================
void SignalChainStrip::showAddMenu(bool side)
{
    const auto list = plugins ? plugins() : juce::Array<juce::PluginDescription>();
    juce::PopupMenu effects, instruments, menu;
    std::vector<juce::PluginDescription> choices;
    for (const auto& description : list)
    {
        choices.push_back(description);
        const auto id = static_cast<int>(choices.size());
        const auto label = description.name + (description.manufacturerName.isNotEmpty() ? "  (" + description.manufacturerName + ")" : juce::String());
        (description.isInstrument ? instruments : effects).addItem(id, label);
    }
    menu.addSectionHeader(side ? text("Neben der Kette einfügen") : text("In die Kette einfügen"));
    menu.addSubMenu(text("Effekt / Audio → MIDI (z. B. TONIQ)"), effects, effects.getNumItems() > 0);
    menu.addSubMenu(text("Instrument (z. B. Vital)"), instruments, instruments.getNumItems() > 0);
    menu.addItem(9001, text("MIDI transponieren"));
    menu.addSeparator();
    menu.addItem(9002, list.isEmpty() ? text("VST3-Plugins suchen … (noch keine gefunden)") : text("VST3-Plugins neu suchen"));
    menu.addItem(9003, text("Plugins verwalten / importieren …"), managePlugins != nullptr);
    const juce::Component::SafePointer<SignalChainStrip> self(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self, side, choices](int result)
    {
        if (self == nullptr || result == 0)
            return;
        if (result == 9003)
        {
            self->managePlugins();
            return;
        }
        if (result == 9002)
        {
            if (self->rescanPlugins)
                self->rescanPlugins([self] { if (self != nullptr) self->refresh(); });
            return;
        }
        auto layout = self->current;
        chain::Node node;
        // Instruments make their own sound: beside the chain, into the output,
        // played by the MIDI keyboards until a tracker is connected.
        const auto instrument = result != 9001 && choices[static_cast<size_t>(result - 1)].isInstrument;
        const auto beside = side || instrument;
        if (result == 9001)
        {
            node.kind = "transpose";
            node.semitones = 12;
        }
        else
        {
            const auto& description = choices[static_cast<size_t>(result - 1)];
            node.kind = "vst3";
            node.name = description.name;
            juce::ValueTree state("Vst3Slot");
            state.setProperty("version", 1, nullptr);
            state.setProperty("descriptionXml", description.createXml()->toString(), nullptr);
            state.setProperty("stateBase64", juce::String(), nullptr);
            node.plugin = state;
        }
        const auto id = layout.add(node, beside ? -1 : static_cast<int>(layout.serial.size()));
        if (instrument)
        {
            layout.addLink({ id, chain::output, false, 0.0f });
            if (self->offerKeyboards)
                layout.addLink({ chain::keys, id, true, 0.0f });
        }
        else if (side && node.kind == "vst3")
            layout.addLink({ chain::input, id, false, 0.0f }); // e.g. a tracker listening to the guitar
        self->change(layout);
    });
}

void SignalChainStrip::showBlockMenu(const juce::String& id)
{
    const auto* node = current.find(id);
    const auto isTerminal = node == nullptr;
    const auto canSendMidi = id == chain::keys || (node != nullptr && node->handlesMidi());
    const auto canReceiveMidi = node != nullptr && node->handlesMidi();
    juce::PopupMenu menu, sendAudio, sendMidi, receiveAudio, receiveMidi;
    std::vector<chain::Link> actions;
    const auto addAction = [&actions](juce::PopupMenu& target, const juce::String& label, chain::Link link, bool exists)
    {
        actions.push_back(link);
        target.addItem(static_cast<int>(actions.size()), label, true, exists);
    };
    const auto exists = [this](const chain::Link& link)
    {
        return std::any_of(current.links.begin(), current.links.end(),
                           [&link](const chain::Link& l) { return l.from == link.from && l.to == link.to && l.midi == link.midi; });
    };
    const auto everyBlock = [this](const std::function<void(const chain::Node&)>& visit)
    {
        for (const auto* list : { &current.serial, &current.side })
            for (const auto& other : *list)
                visit(other);
    };
    if (id != chain::output && id != chain::keys)
    {
        everyBlock([&](const chain::Node& other)
        {
            if (other.id != id)
            {
                const chain::Link link { id, other.id, false, 0.0f };
                addAction(sendAudio, nameOf(other.id), link, exists(link));
            }
        });
        sendAudio.addSeparator();
        const chain::Link toOutput { id, chain::output, false, 0.0f };
        addAction(sendAudio, text("Ausgang"), toOutput, exists(toOutput));
    }
    if (canSendMidi)
        everyBlock([&](const chain::Node& other)
        {
            if (other.id != id && other.handlesMidi())
            {
                const chain::Link link { id, other.id, true, 0.0f };
                addAction(sendMidi, nameOf(other.id), link, exists(link));
            }
        });
    if (node != nullptr)
    {
        const chain::Link fromGuitar { chain::input, id, false, 0.0f };
        addAction(receiveAudio, text("Gitarre (trocken)"), fromGuitar, exists(fromGuitar));
        receiveAudio.addSeparator();
        everyBlock([&](const chain::Node& other)
        {
            if (other.id != id)
            {
                const chain::Link link { other.id, id, false, 0.0f };
                addAction(receiveAudio, nameOf(other.id), link, exists(link));
            }
        });
    }
    if (canReceiveMidi)
    {
        if (offerKeyboards)
        {
            const chain::Link fromKeys { chain::keys, id, true, 0.0f };
            addAction(receiveMidi, text("MIDI-Keyboards"), fromKeys, exists(fromKeys));
            receiveMidi.addSeparator();
        }
        everyBlock([&](const chain::Node& other)
        {
            if (other.id != id && other.handlesMidi())
            {
                const chain::Link link { other.id, id, true, 0.0f };
                addAction(receiveMidi, nameOf(other.id), link, exists(link));
            }
        });
    }

    menu.addSectionHeader(nameOf(id));
    if (node != nullptr)
        menu.addItem(10001, node->kind == "vst3" ? text("Plugin-Fenster öffnen") : text("Einstellungen öffnen"), onOpen != nullptr);
    if (sendAudio.getNumItems() > 0) menu.addSubMenu(text("Audio senden an"), sendAudio);
    if (sendMidi.getNumItems() > 0) menu.addSubMenu(text("MIDI senden an"), sendMidi);
    if (receiveAudio.getNumItems() > 0) menu.addSubMenu(text("Audio empfangen von"), receiveAudio);
    if (receiveMidi.getNumItems() > 0) menu.addSubMenu(text("MIDI empfangen von"), receiveMidi);
    menu.addItem(10002, text("Verbindungen und Pegel …"));
    if (id == chain::output)
    {
        juce::PopupMenu level;
        for (const auto db : { 0.0f, -6.0f, -12.0f, -60.0f })
            level.addItem(20000 + juce::roundToInt(-db), db <= -59.9f ? text("aus (nur Verbindungen, z. B. nur Synth)") : juce::String(db, 0) + " dB",
                          true, std::abs(current.chainLevelDb - db) < 0.05f);
        menu.addSubMenu(text("Pegel der Kette in den Ausgang"), level);
    }
    if (node != nullptr && ! node->isBuiltin())
    {
        menu.addSeparator();
        if (current.isSerial(id))
            menu.addItem(10003, text("Neben die Kette stellen"));
        else
            menu.addItem(10004, text("In die Kette (ans Ende) stellen"));
        if (node->kind == "transpose")
        {
            juce::PopupMenu shift;
            for (const auto semitones : { -24, -12, -7, -5, 5, 7, 12, 24 })
                shift.addItem(30000 + semitones, (semitones > 0 ? "+" : "") + juce::String(semitones) + text(" Halbtöne"), true, node->semitones == semitones);
            menu.addSubMenu(text("Transponieren um"), shift);
        }
        menu.addItem(10005, text("Entfernen"));
    }

    const juce::Component::SafePointer<SignalChainStrip> self(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self, id, actions](int result)
    {
        if (self == nullptr || result == 0)
            return;
        auto layout = self->current;
        if (juce::isPositiveAndNotGreaterThan(result, static_cast<int>(actions.size())))
        {
            const auto& link = actions[static_cast<size_t>(result - 1)];
            const auto present = std::any_of(layout.links.begin(), layout.links.end(),
                                             [&link](const chain::Link& l) { return l.from == link.from && l.to == link.to && l.midi == link.midi; });
            if (present)
                layout.removeLink(link); // the ticked entry toggles the connection off
            else
                layout.addLink(link);
        }
        else if (result == 10001) { if (self->onOpen) self->onOpen(id); return; }
        else if (result == 10002) { self->showConnections(id == chain::output ? juce::String() : id); return; }
        else if (result == 10003) layout.moveToSide(id);
        else if (result == 10004) layout.moveTo(id, static_cast<int>(layout.serial.size()));
        else if (result == 10005) layout.remove(id);
        else if (result >= 20000 && result <= 20060) layout.chainLevelDb = -static_cast<float>(result - 20000);
        else if (result >= 29964 && result <= 30024)
        {
            if (auto* node = layout.find(id))
                node->semitones = result - 30000;
        }
        self->change(layout);
    });
}

void SignalChainStrip::showConnections(const juce::String& id)
{
    const juce::Component::SafePointer<SignalChainStrip> self(this);
    auto panel = std::make_unique<ConnectionsPanel>(current, id, [self](const juce::String& endpoint)
    {
        return self != nullptr ? self->nameOf(endpoint) : endpoint;
    }, [self](const chain::Layout& layout)
    {
        if (self != nullptr)
            self->change(layout);
    });
    juce::CallOutBox::launchAsynchronously(std::move(panel), id.isEmpty() ? getScreenBounds() : screenBoundsOf(id), nullptr);
}

juce::String SignalChainStrip::getTooltip()
{
    const auto index = itemAt(getMouseXYRelative().toFloat());
    if (index < 0)
        return text("Rechtsklick: alle Verbindungen");
    const auto& item = items[static_cast<size_t>(index)];
    switch (item.type)
    {
        case ItemType::input: return text("Gitarre (nach dem Input-Regler). Rechtsklick: an weitere Blöcke senden");
        case ItemType::output: return text("Ausgang. Rechtsklick: Pegel der Kette, Verbindungen");
        case ItemType::keys: return text("MIDI-Keyboards und virtuelle MIDI-Ports. Rechtsklick: an Instrumente senden");
        case ItemType::addSerial: return text("Plugin oder MIDI-Werkzeug in die Kette einfügen");
        case ItemType::addSide: return text("Plugin neben der Kette einfügen (z. B. Synth oder Audio → MIDI)");
        case ItemType::block: break;
    }
    const auto it = described.find(item.id);
    auto tip = it != described.end() ? it->second.tooltip : juce::String();
    const auto toggle = it != described.end() && it->second.toggleable && ! it->second.empty;
    tip << (tip.isEmpty() ? "" : "\n") << (toggle ? text("Punkt: an/aus · Klick: öffnen · Ziehen: verschieben · Rechtsklick: verbinden")
                                                  : text("Klick: öffnen · Ziehen: verschieben · Rechtsklick: verbinden"));
    return tip;
}
} // namespace nam::ui
