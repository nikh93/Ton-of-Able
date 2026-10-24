#include "ExportRangeBar.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using daw::text;
using Theme = AmberTheme;

namespace
{
const juce::Colour rangeColour { 0xff5fb3a6 }; // distinct from the amber loop
constexpr float grab = 7.0f;
} // namespace

ExportRangeBar::ExportRangeBar(daw::ArrangementController& c, const TimelineComponent& t, int width)
    : controller(c), timeline(t), labelWidth(width)
{
    setTooltip(text("Export-Bereich: Anfang und Ende ziehen, Mitte verschiebt, Doppelklick = ganzer Song. "
                    "Gilt für Mix, Stems und einzelne Spuren."));
}

float ExportRangeBar::toX(juce::int64 sample) const
{
    return static_cast<float>(labelWidth) + timeline.sampleToX(static_cast<double>(sample));
}

juce::int64 ExportRangeBar::toSample(float x) const
{
    return std::max<juce::int64>(0, timeline.xToSample(x - static_cast<float>(labelWidth)));
}

ExportRangeBar::Part ExportRangeBar::partAt(float x) const
{
    const auto [from, to] = controller.state().exportRange();
    if (to <= from || x < static_cast<float>(labelWidth))
        return Part::none;
    const auto x0 = toX(from), x1 = toX(to);
    const auto reach = std::min(grab, (x1 - x0) / 3.0f);
    if (std::abs(x - x0) <= std::max(reach, 3.0f) && x < x0 + reach)
        return Part::start;
    if (std::abs(x - x1) <= std::max(reach, 3.0f) && x > x1 - reach)
        return Part::end;
    return x > x0 && x < x1 ? Part::body : Part::none;
}

void ExportRangeBar::update()
{
    const auto& state = controller.state();
    const auto now = std::make_tuple(timeline.viewStart(), timeline.samplesPerPixel(), state.exportStart, state.exportEnd, state.endSample());
    if (now != painted)
        repaint();
}

void ExportRangeBar::paint(juce::Graphics& g)
{
    const auto& state = controller.state();
    painted = std::make_tuple(timeline.viewStart(), timeline.samplesPerPixel(), state.exportStart, state.exportEnd, state.endSample());
    g.fillAll(Theme::panel());
    const auto [from, to] = state.exportRange();
    const auto explicitRange = state.hasExportRange();
    const auto grid = controller.grid();

    auto label = getLocalBounds().removeFromLeft(labelWidth).reduced(10, 0);
    g.setColour(Theme::subduedText());
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(text("EXPORT-BEREICH"), label, juce::Justification::centredLeft);

    g.saveState();
    g.reduceClipRegion(getLocalBounds().withTrimmedLeft(labelWidth));
    if (to > from)
    {
        const auto x0 = toX(from), x1 = toX(to);
        const auto bar = juce::Rectangle<float>(x0, 3.0f, x1 - x0, static_cast<float>(getHeight()) - 6.0f);
        g.setColour(explicitRange ? rangeColour : rangeColour.withAlpha(0.35f));
        g.fillRoundedRectangle(bar, 3.0f);
        // Grips, as on the loop bar.
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        if (bar.getWidth() > 12.0f)
            for (const auto x : { x0 + 3.0f, x1 - 4.0f })
                g.fillRect(juce::Rectangle<float>(x, bar.getY() + 3.0f, 1.0f, bar.getHeight() - 6.0f));
        const auto caption = (explicitRange ? text("Export ") : text("Ganzer Song "))
                           + grid.formatPosition(from) + text(" – ") + grid.formatPosition(to);
        g.setColour(explicitRange ? Theme::onText() : Theme::text());
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(caption, bar.reduced(10.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, true);
    }
    g.restoreState();
}

void ExportRangeBar::mouseMove(const juce::MouseEvent& event)
{
    const auto part = partAt(event.position.x);
    setMouseCursor(part == Part::start || part == Part::end ? juce::MouseCursor::LeftRightResizeCursor
                   : part == Part::body ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void ExportRangeBar::mouseDown(const juce::MouseEvent& event)
{
    const auto [from, to] = controller.state().exportRange();
    dragging = partAt(event.position.x);
    anchor = toSample(event.position.x);
    startAtDown = from;
    endAtDown = to;
    if (dragging == Part::none && event.position.x >= static_cast<float>(labelWidth))
    {
        // Outside the bar: draw a new range from here.
        dragging = Part::end;
        startAtDown = endAtDown = timeline.snapPosition(anchor, event.mods);
    }
}

void ExportRangeBar::mouseDrag(const juce::MouseEvent& event)
{
    const auto at = toSample(event.position.x);
    switch (dragging)
    {
        case Part::start:
        case Part::end:
        {
            const auto edge = timeline.snapPosition(at, event.mods);
            const auto fixed = dragging == Part::start ? endAtDown : startAtDown;
            if (edge != fixed)
                controller.setExportRange(std::min(edge, fixed), std::max(edge, fixed));
            break;
        }
        case Part::body:
        {
            const auto length = endAtDown - startAtDown;
            const auto start = timeline.snapPosition(std::max<juce::int64>(0, startAtDown + at - anchor), event.mods);
            controller.setExportRange(start, start + length);
            break;
        }
        case Part::none: break;
    }
    repaint();
}

void ExportRangeBar::mouseUp(const juce::MouseEvent& event)
{
    if (dragging != Part::none && event.mouseWasDraggedSinceMouseDown())
        mouseDrag(event); // the release position counts
    dragging = Part::none;
}

void ExportRangeBar::mouseDoubleClick(const juce::MouseEvent&)
{
    controller.setExportRange(0, 0); // the whole song again
    repaint();
}
} // namespace nam::ui
