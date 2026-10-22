#include "TimelineComponent.h"

#include <cmath>

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
namespace
{
using Theme = AmberTheme;
using daw::text;

constexpr float edgeZone = 7.0f;
constexpr float fadeHandleZone = 12.0f;
constexpr double minimumZoom = 2.0;      // samples per pixel
constexpr double maximumZoom = 40000.0;

struct ClipboardEntry
{
    daw::Clip clip;
    int trackOffset = 0;
    juce::int64 relativeStart = 0;
};
std::vector<ClipboardEntry>& clipboard()
{
    static std::vector<ClipboardEntry> entries;
    return entries;
}

juce::String percent(double ratio)
{
    const auto value = ratio * 100.0;
    return juce::String(value, std::abs(value - std::round(value)) < 0.05 ? 0 : 1) + "%";
}
} // namespace

TimelineComponent::TimelineComponent(daw::ArrangementController& c) : controller(c)
{
    setWantsKeyboardFocus(true);
    setRepaintsOnMouseActivity(false);
}

//==============================================================================
juce::int64 TimelineComponent::xToSample(float x) const noexcept
{
    return std::max<juce::int64>(0, static_cast<juce::int64>(std::llround(xToSampleExact(x))));
}

int TimelineComponent::trackAt(int y) const noexcept
{
    if (y < rulerHeight)
        return -1;
    const auto index = (y - rulerHeight + verticalOffset) / laneHeight;
    return index < static_cast<int>(controller.state().tracks.size()) ? index : static_cast<int>(controller.state().tracks.size());
}

juce::Rectangle<float> TimelineComponent::clipBounds(const daw::Clip& clip, int track) const noexcept
{
    const auto x0 = sampleToX(static_cast<double>(clip.start));
    const auto x1 = sampleToX(static_cast<double>(clip.end()));
    return { x0, static_cast<float>(laneTop(track) + 3), std::max(2.0f, x1 - x0), static_cast<float>(laneHeight - 6) };
}

TimelineComponent::Hit TimelineComponent::hitTest(juce::Point<float> point) const
{
    Hit hit;
    hit.track = trackAt(static_cast<int>(point.y));
    const auto& tracks = controller.state().tracks;
    if (hit.track < 0 || hit.track >= static_cast<int>(tracks.size()))
        return hit;
    for (const auto& clip : tracks[static_cast<size_t>(hit.track)].clips)
    {
        const auto bounds = clipBounds(clip, hit.track);
        if (! bounds.expanded(1.0f, 0.0f).contains(point))
            continue;
        hit.clip = &clip;
        const auto fadeInX = bounds.getX() + static_cast<float>(static_cast<double>(clip.fadeIn) / zoom);
        const auto fadeOutX = bounds.getRight() - static_cast<float>(static_cast<double>(clip.fadeOut) / zoom);
        const auto inHandleRow = point.y < bounds.getY() + fadeHandleZone;
        if (inHandleRow && std::abs(point.x - fadeInX) < 6.0f && bounds.getWidth() > 30.0f)
            hit.zone = Zone::fadeIn;
        else if (inHandleRow && std::abs(point.x - fadeOutX) < 6.0f && bounds.getWidth() > 30.0f)
            hit.zone = Zone::fadeOut;
        else if (point.x < bounds.getX() + edgeZone && bounds.getWidth() > 3 * edgeZone)
            hit.zone = Zone::leftEdge;
        else if (point.x > bounds.getRight() - edgeZone && bounds.getWidth() > 3 * edgeZone)
            hit.zone = Zone::rightEdge;
        else
            hit.zone = Zone::body;
        return hit;
    }
    return hit;
}

juce::int64 TimelineComponent::snap(juce::int64 position, const juce::ModifierKeys& mods) const
{
    // Alt temporarily disables the grid, as in Ableton.
    if (mods.isAltDown() || snapBars <= 0.0)
        return std::max<juce::int64>(0, position);
    return std::max<juce::int64>(0, controller.grid().snap(position, snapBars));
}

double TimelineComponent::gridStepBars() const
{
    const auto bar = controller.grid().samplesPerBar();
    auto step = snapBars > 0.0 ? snapBars : 0.25;
    while (step * bar / zoom < 9.0)
        step *= 2.0;
    while (step * bar / zoom > 90.0 && step > 1.0 / 32.0)
        step *= 0.5;
    return step;
}

int TimelineComponent::contentHeight() const
{
    return static_cast<int>(controller.state().tracks.size()) * laneHeight + laneHeight;
}

double TimelineComponent::contentLengthSamples() const
{
    const auto& state = controller.state();
    const auto end = std::max<double>(static_cast<double>(state.endSample()), static_cast<double>(controller.engine().position()));
    return end + controller.grid().samplesPerBar() * 8.0;
}

//==============================================================================
void TimelineComponent::setTool(Tool newTool)
{
    currentTool = newTool;
    setMouseCursor(newTool == Tool::split ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor);
}

void TimelineComponent::zoomBy(double factor, float anchorX)
{
    const auto anchor = xToSampleExact(anchorX);
    zoom = juce::jlimit(minimumZoom, maximumZoom, zoom * factor);
    firstSample = std::max(0.0, anchor - static_cast<double>(anchorX) * zoom);
    notifyView();
}

void TimelineComponent::zoomToFit()
{
    const auto end = std::max<double>(static_cast<double>(controller.state().endSample()), controller.grid().samplesPerBar() * 8.0);
    zoom = juce::jlimit(minimumZoom, maximumZoom, end * 1.05 / std::max(100, getWidth()));
    firstSample = 0.0;
    notifyView();
}

void TimelineComponent::setTrackHeight(int height)
{
    laneHeight = juce::jlimit(44, 260, height);
    setScrollY(verticalOffset);
}

void TimelineComponent::setScrollY(int value)
{
    verticalOffset = juce::jlimit(0, std::max(0, contentHeight() - (getHeight() - rulerHeight)), value);
    notifyView();
}

void TimelineComponent::setViewStart(double sample)
{
    firstSample = std::max(0.0, sample);
    notifyView();
}

void TimelineComponent::notifyView()
{
    repaint();
    if (onViewChanged)
        onViewChanged();
}

void TimelineComponent::followPlayhead()
{
    if (drag != Drag::none || ! controller.engine().isPlaying())
        return;
    const auto x = sampleToX(static_cast<double>(controller.engine().position()));
    if (x > static_cast<float>(getWidth()) * 0.92f || x < 0.0f)
        setViewStart(static_cast<double>(controller.engine().position()) - getWidth() * 0.08 * zoom);
}

void TimelineComponent::stateChanged()
{
    const auto& state = controller.state();
    selection.removeIf([&](const juce::Uuid& id) { return state.findClip(id) == nullptr; });
    currentTrack = juce::jlimit(0, std::max(0, static_cast<int>(state.tracks.size()) - 1), currentTrack);
    repaint();
}

//==============================================================================
void TimelineComponent::selectTrack(int index)
{
    currentTrack = juce::jlimit(0, std::max(0, static_cast<int>(controller.state().tracks.size()) - 1), index);
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void TimelineComponent::selectAll()
{
    selection.clear();
    for (const auto& track : controller.state().tracks)
        for (const auto& clip : track.clips)
            selection.add(clip.id);
    repaint();
}

void TimelineComponent::splitAtPlayhead()
{
    const auto at = controller.engine().position();
    std::vector<juce::Uuid> targets;
    for (size_t index = 0; index < controller.state().tracks.size(); ++index)
        for (const auto& clip : controller.state().tracks[index].clips)
            if (at > clip.start && at < clip.end() && (isSelected(clip.id) || (selection.isEmpty() && static_cast<int>(index) == currentTrack)))
                targets.push_back(clip.id);
    if (targets.empty())
        return;
    controller.perform("Teilen", [&](daw::ArrangementState& state)
    {
        for (const auto& id : targets)
            if (const auto right = daw::edit::split(state, id, at))
                selection.addIfNotAlreadyThere(*right);
    });
}

void TimelineComponent::deleteSelection()
{
    if (selection.isEmpty())
        return;
    const auto ids = selection;
    controller.perform(text("Löschen"), [&](daw::ArrangementState& state)
    {
        for (const auto& id : ids)
            daw::edit::remove(state, id);
    });
    selection.clear();
}

void TimelineComponent::copySelection()
{
    auto& entries = clipboard();
    entries.clear();
    juce::int64 first = std::numeric_limits<juce::int64>::max();
    int firstTrack = std::numeric_limits<int>::max();
    for (const auto& id : selection)
    {
        int track = -1;
        if (const auto* clip = controller.state().findClip(id, &track))
        {
            first = std::min(first, clip->start);
            firstTrack = std::min(firstTrack, track);
            entries.push_back({ *clip, track, clip->start });
        }
    }
    for (auto& entry : entries)
    {
        entry.trackOffset -= firstTrack;
        entry.relativeStart -= first;
    }
}

void TimelineComponent::cutSelection()
{
    copySelection();
    deleteSelection();
}

void TimelineComponent::paste()
{
    const auto& entries = clipboard();
    if (entries.empty())
        return;
    const auto at = controller.engine().position();
    juce::Array<juce::Uuid> pasted;
    controller.perform(text("Einfügen"), [&](daw::ArrangementState& state)
    {
        for (const auto& entry : entries)
        {
            const auto track = currentTrack + entry.trackOffset;
            if (track >= static_cast<int>(state.tracks.size()))
                continue;
            auto clip = entry.clip;
            clip.start = at + entry.relativeStart;
            pasted.add(daw::edit::insert(state, std::move(clip), track));
        }
    });
    selection = pasted;
    repaint();
}

void TimelineComponent::duplicateSelection()
{
    if (selection.isEmpty())
        return;
    juce::int64 first = std::numeric_limits<juce::int64>::max(), last = 0;
    for (const auto& id : selection)
        if (const auto* clip = controller.state().findClip(id))
        {
            first = std::min(first, clip->start);
            last = std::max(last, clip->end());
        }
    const auto shift = last - first;
    juce::Array<juce::Uuid> copies;
    const auto ids = selection;
    controller.perform("Duplizieren", [&](daw::ArrangementState& state)
    {
        for (const auto& id : ids)
        {
            int track = -1;
            if (const auto* clip = state.findClip(id, &track))
            {
                auto copy = *clip;
                copy.start += shift;
                copies.add(daw::edit::insert(state, std::move(copy), track));
            }
        }
    });
    selection = copies;
    repaint();
}

void TimelineComponent::stretchSelection(double ratio)
{
    if (selection.isEmpty())
        return;
    const auto ids = selection;
    controller.perform("Time-Stretch " + percent(ratio), [&](daw::ArrangementState& state)
    {
        for (const auto& id : ids)
            daw::edit::setStretchRatio(state, id, ratio);
    });
}

void TimelineComponent::transposeSelection(double semitones, bool relative)
{
    if (selection.isEmpty())
        return;
    const auto ids = selection;
    controller.perform("Transponieren", [&](daw::ArrangementState& state)
    {
        for (const auto& id : ids)
            if (const auto* clip = state.findClip(id))
                daw::edit::setPitch(state, id, relative ? clip->pitchSemitones + semitones : semitones);
    });
}

//==============================================================================
void TimelineComponent::paint(juce::Graphics& g)
{
    const auto& state = controller.state();
    g.fillAll(juce::Colour(0xff1b1714));
    auto lanes = getLocalBounds().withTrimmedTop(rulerHeight);
    {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(lanes);
        for (size_t index = 0; index < state.tracks.size(); ++index)
        {
            const auto top = laneTop(static_cast<int>(index));
            if (top > getHeight() || top + laneHeight < rulerHeight)
                continue;
            const auto lane = juce::Rectangle<int>(0, top, getWidth(), laneHeight);
            g.setColour(static_cast<int>(index) == currentTrack ? juce::Colour(0xff2a221b) : (index % 2 == 0 ? juce::Colour(0xff211b16) : juce::Colour(0xff1e1915)));
            g.fillRect(lane);
            if (state.tracks[index].armed)
            {
                g.setColour(Theme::danger().withAlpha(0.06f));
                g.fillRect(lane);
            }
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.drawHorizontalLine(lane.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));
        }
        paintGrid(g, lanes);

        if (state.loopEnabled && state.loopEnd > state.loopStart)
        {
            g.setColour(Theme::amber().withAlpha(0.05f));
            g.fillRect(juce::Rectangle<float>(sampleToX(static_cast<double>(state.loopStart)), static_cast<float>(rulerHeight),
                                              sampleToX(static_cast<double>(state.loopEnd)) - sampleToX(static_cast<double>(state.loopStart)),
                                              static_cast<float>(getHeight() - rulerHeight)));
        }

        for (size_t index = 0; index < state.tracks.size(); ++index)
        {
            const auto top = laneTop(static_cast<int>(index));
            if (top > getHeight() || top + laneHeight < rulerHeight)
                continue;
            for (const auto& clip : state.tracks[index].clips)
            {
                const auto bounds = clipBounds(clip, static_cast<int>(index));
                if (bounds.getRight() < 0.0f || bounds.getX() > static_cast<float>(getWidth()))
                    continue;
                paintClip(g, clip, static_cast<int>(index), state.tracks[index]);
            }
        }

        // Growing take while recording.
        auto& engine = controller.engine();
        if (engine.isRecording() && engine.punchInPosition() != daw::ArrangementEngine::punchAtNextBlock && engine.position() > engine.punchInPosition())
        {
            const auto x0 = sampleToX(static_cast<double>(engine.punchInPosition()));
            const auto x1 = sampleToX(static_cast<double>(engine.position()));
            for (size_t index = 0; index < state.tracks.size(); ++index)
                if (state.tracks[index].armed)
                {
                    const auto area = juce::Rectangle<float>(x0, static_cast<float>(laneTop(static_cast<int>(index)) + 3), x1 - x0, static_cast<float>(laneHeight - 6));
                    g.setColour(Theme::danger().withAlpha(0.35f));
                    g.fillRoundedRectangle(area, 4.0f);
                    g.setColour(Theme::danger());
                    g.drawRoundedRectangle(area, 4.0f, 1.2f);
                    g.setColour(juce::Colours::white);
                    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                    g.drawText("REC", area.reduced(6.0f, 2.0f).removeFromTop(14.0f), juce::Justification::topLeft);
                }
        }

        if (drag == Drag::marquee)
        {
            g.setColour(Theme::amber().withAlpha(0.12f));
            g.fillRect(marquee);
            g.setColour(Theme::amber());
            g.drawRect(marquee, 1.0f);
        }

        if (dropTrack > -2)
        {
            g.setColour(Theme::amberBright());
            g.drawVerticalLine(static_cast<int>(dropX), static_cast<float>(rulerHeight), static_cast<float>(getHeight()));
            const auto y = dropTrack >= 0 && dropTrack < static_cast<int>(state.tracks.size()) ? laneTop(dropTrack) : laneTop(static_cast<int>(state.tracks.size()));
            g.setColour(Theme::amber().withAlpha(0.15f));
            g.fillRect(juce::Rectangle<float>(dropX, static_cast<float>(y), static_cast<float>(getWidth()) - dropX, static_cast<float>(laneHeight)));
        }
    }

    paintRuler(g);

    const auto playX = sampleToX(static_cast<double>(controller.engine().position()));
    if (playX >= 0.0f && playX <= static_cast<float>(getWidth()))
    {
        g.setColour(controller.engine().isRecording() ? Theme::danger() : Theme::amberBright());
        g.drawLine(playX, 0.0f, playX, static_cast<float>(getHeight()), 1.4f);
        juce::Path head;
        head.addTriangle(playX - 6.0f, static_cast<float>(rulerHeight) - 10.0f, playX + 6.0f, static_cast<float>(rulerHeight) - 10.0f, playX, static_cast<float>(rulerHeight));
        g.fillPath(head);
    }
}

void TimelineComponent::paintGrid(juce::Graphics& g, juce::Rectangle<int> lanes)
{
    const auto grid = controller.grid();
    const auto step = gridStepBars() * grid.samplesPerBar();
    const auto bar = grid.samplesPerBar();
    auto index = std::floor(firstSample / step);
    for (auto position = index * step; position <= xToSampleExact(static_cast<float>(getWidth())); position += step)
    {
        const auto x = sampleToX(position);
        const auto barPhase = std::fmod(position, bar);
        const auto isBar = barPhase < 0.5 || bar - barPhase < 0.5;
        g.setColour(juce::Colours::white.withAlpha(isBar ? 0.09f : 0.035f));
        g.drawVerticalLine(static_cast<int>(std::round(x)), static_cast<float>(lanes.getY()), static_cast<float>(lanes.getBottom()));
    }
}

void TimelineComponent::paintRuler(juce::Graphics& g)
{
    const auto& state = controller.state();
    const auto grid = controller.grid();
    auto ruler = getLocalBounds().removeFromTop(rulerHeight);
    g.setColour(juce::Colour(0xff2b231c));
    g.fillRect(ruler);
    auto loopStrip = ruler.removeFromTop(12);
    g.setColour(juce::Colour(0xff221c17));
    g.fillRect(loopStrip);

    if (state.loopEnd > state.loopStart)
    {
        const auto x0 = sampleToX(static_cast<double>(state.loopStart));
        const auto x1 = sampleToX(static_cast<double>(state.loopEnd));
        g.setColour(state.loopEnabled ? Theme::amber() : Theme::subduedText().withAlpha(0.5f));
        g.fillRoundedRectangle(juce::Rectangle<float>(x0, 1.0f, x1 - x0, 10.0f), 3.0f);
    }

    const auto bar = grid.samplesPerBar();
    const auto pixelsPerBar = bar / zoom;
    auto labelEvery = 1;
    while (pixelsPerBar * labelEvery < 38.0)
        labelEvery *= 2;
    g.setFont(juce::FontOptions(11.0f));
    const auto firstBar = static_cast<int>(std::floor(firstSample / bar));
    for (auto number = firstBar; static_cast<double>(number) * bar <= xToSampleExact(static_cast<float>(getWidth())); ++number)
    {
        const auto x = sampleToX(static_cast<double>(number) * bar);
        const auto labelled = number % labelEvery == 0;
        g.setColour(Theme::subduedText().withAlpha(labelled ? 0.8f : 0.3f));
        g.drawVerticalLine(static_cast<int>(std::round(x)), static_cast<float>(ruler.getY() + (labelled ? 0 : 12)), static_cast<float>(ruler.getBottom()));
        if (labelled)
            g.drawText(juce::String(number + 1), static_cast<int>(x) + 3, ruler.getY(), 40, 14, juce::Justification::left);
        if (pixelsPerBar > 60.0)
            for (int beat = 1; beat < grid.beatsPerBar; ++beat)
            {
                const auto bx = sampleToX(static_cast<double>(number) * bar + beat * grid.samplesPerBeat());
                g.setColour(Theme::subduedText().withAlpha(0.25f));
                g.drawVerticalLine(static_cast<int>(std::round(bx)), static_cast<float>(ruler.getBottom() - 6), static_cast<float>(ruler.getBottom()));
            }
    }
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawHorizontalLine(rulerHeight - 1, 0.0f, static_cast<float>(getWidth()));
}

void TimelineComponent::paintClip(juce::Graphics& g, const daw::Clip& clip, int track, const daw::Track& owner)
{
    const auto bounds = clipBounds(clip, track);
    const auto selected = isSelected(clip.id);
    const auto colour = owner.colour;
    g.setColour(colour.withMultipliedBrightness(0.32f).withAlpha(0.95f));
    g.fillRoundedRectangle(bounds, 4.0f);
    auto header = bounds.withHeight(std::min(16.0f, bounds.getHeight()));
    g.setColour(colour.withAlpha(selected ? 0.95f : 0.7f));
    g.fillRoundedRectangle(header, 4.0f);

    // Waveform from the original peaks, mapped through offset and stretch.
    if (clip.source != nullptr && bounds.getHeight() > 24.0f)
    {
        const auto waveArea = bounds.withTrimmedTop(header.getHeight()).reduced(0.0f, 2.0f);
        const auto centre = waveArea.getCentreY();
        const auto half = waveArea.getHeight() * 0.5f;
        const auto gain = juce::Decibels::decibelsToGain(clip.gainDb);
        g.setColour(colour.brighter(0.5f).withAlpha(0.85f));
        const auto left = std::max(0, static_cast<int>(std::floor(bounds.getX())));
        const auto right = std::min(getWidth(), static_cast<int>(std::ceil(bounds.getRight())));
        juce::RectangleList<float> columns;
        for (int x = left; x < right; ++x)
        {
            const auto t0 = static_cast<juce::int64>(xToSampleExact(static_cast<float>(x)));
            const auto t1 = static_cast<juce::int64>(xToSampleExact(static_cast<float>(x + 1)));
            const auto from = std::max(clip.start, t0);
            const auto to = std::min(clip.end(), t1);
            if (to <= from)
                continue;
            auto peak = clip.source->peakBetween(clip.sourcePositionAt(from), clip.sourcePositionAt(to)) * gain;
            const auto local = from - clip.start;
            if (clip.fadeIn > 0 && local < clip.fadeIn)
                peak *= static_cast<float>(local) / static_cast<float>(clip.fadeIn);
            if (clip.fadeOut > 0 && clip.end() - from < clip.fadeOut)
                peak *= static_cast<float>(clip.end() - from) / static_cast<float>(clip.fadeOut);
            const auto height = std::max(0.5f, std::min(1.0f, peak) * half);
            columns.addWithoutMerging({ static_cast<float>(x), centre - height, 1.0f, height * 2.0f });
        }
        g.fillRectList(columns);
    }

    // Fades.
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    const auto fadeInX = bounds.getX() + static_cast<float>(static_cast<double>(clip.fadeIn) / zoom);
    const auto fadeOutX = bounds.getRight() - static_cast<float>(static_cast<double>(clip.fadeOut) / zoom);
    if (clip.fadeIn > 0)
        g.drawLine(bounds.getX(), bounds.getBottom(), fadeInX, bounds.getY() + header.getHeight(), 1.2f);
    if (clip.fadeOut > 0)
        g.drawLine(fadeOutX, bounds.getY() + header.getHeight(), bounds.getRight(), bounds.getBottom(), 1.2f);
    if (selected && bounds.getWidth() > 30.0f)
    {
        g.setColour(juce::Colours::white);
        g.fillRect(juce::Rectangle<float>(5.0f, 5.0f).withCentre({ fadeInX, bounds.getY() + 5.0f }));
        g.fillRect(juce::Rectangle<float>(5.0f, 5.0f).withCentre({ fadeOutX, bounds.getY() + 5.0f }));
    }

    juce::String label = clip.name;
    if (std::abs(clip.stretchRatio - 1.0) > 1.0e-6)
        label << "  " << juce::String(juce::CharPointer_UTF8("\xe2\x86\x94 ")) << percent(clip.stretchRatio);
    if (std::abs(clip.pitchSemitones) > 1.0e-6)
        label << "  " << (clip.pitchSemitones > 0 ? "+" : "") << juce::String(clip.pitchSemitones, 1) << " st";
    if (std::abs(clip.gainDb) > 0.05f)
        label << "  " << juce::String(clip.gainDb, 1) << " dB";
    if (clip.source == nullptr)
        label << text("  · lädt …");
    else if (controller.isRendering(clip))
        label << text("  · Rubber Band rechnet …");
    g.setColour(juce::Colour(0xff1a140f));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText(label, header.reduced(5.0f, 0.0f), juce::Justification::centredLeft, true);

    g.setColour(selected ? Theme::amberBright() : juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, selected ? 1.8f : 1.0f);
}

//==============================================================================
void TimelineComponent::mouseMove(const juce::MouseEvent& event)
{
    if (event.y < rulerHeight)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }
    if (currentTool == Tool::split)
    {
        setMouseCursor(juce::MouseCursor::IBeamCursor);
        return;
    }
    const auto hit = hitTest(event.position);
    switch (hit.zone)
    {
        case Zone::leftEdge:
        case Zone::rightEdge:
            setMouseCursor(currentTool == Tool::stretch || event.mods.isCommandDown() ? juce::MouseCursor::UpDownLeftRightResizeCursor
                                                                                      : juce::MouseCursor::LeftRightResizeCursor);
            break;
        case Zone::fadeIn:
        case Zone::fadeOut: setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
        case Zone::body: setMouseCursor(juce::MouseCursor::DraggingHandCursor); break;
        case Zone::none: setMouseCursor(juce::MouseCursor::NormalCursor); break;
    }
}

void TimelineComponent::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    mouseDownPosition = event.position;
    dragMoved = false;
    drag = Drag::none;
    const auto& state = controller.state();
    const auto at = xToSample(event.position.x);

    if (event.y < rulerHeight)
    {
        if (event.mods.isPopupMenu())
            return;
        if (event.y < 12)
        {
            const auto inside = state.loopEnd > state.loopStart && at >= state.loopStart && at <= state.loopEnd;
            drag = inside ? Drag::loopMove : Drag::loopCreate;
            dragAnchorSample = inside ? at : snap(at, event.mods);
            loopDragStart = state.loopStart;
            loopDragEnd = state.loopEnd;
        }
        else
        {
            drag = Drag::scrub;
            controller.locate(snap(at, event.mods));
        }
        return;
    }

    const auto hit = hitTest(event.position);
    if (hit.track >= 0 && hit.track < static_cast<int>(state.tracks.size()))
        selectTrack(hit.track);

    if (event.mods.isPopupMenu())
    {
        if (hit.clip != nullptr)
        {
            if (! isSelected(hit.clip->id))
                selection = { hit.clip->id };
            repaint();
            showClipMenu(*hit.clip, snap(at, event.mods));
        }
        else
            showLaneMenu(hit.track, snap(at, event.mods));
        return;
    }

    if (hit.clip == nullptr)
    {
        if (! event.mods.isShiftDown())
            selection.clear();
        drag = Drag::marquee;
        marquee = {};
        repaint();
        return;
    }

    if (currentTool == Tool::split)
    {
        const auto id = hit.clip->id;
        const auto position = snap(at, event.mods);
        controller.perform("Teilen", [&](daw::ArrangementState& s) { daw::edit::split(s, id, position); });
        return;
    }

    dragClip = hit.clip->id;
    dragClipStart = hit.clip->start;
    dragAnchorSample = at;
    dragAnchorTrack = hit.track;
    const auto stretching = currentTool == Tool::stretch || event.mods.isCommandDown();
    switch (hit.zone)
    {
        case Zone::leftEdge: drag = stretching ? Drag::stretchStart : Drag::trimStart; break;
        case Zone::rightEdge: drag = stretching ? Drag::stretchEnd : Drag::trimEnd; break;
        case Zone::fadeIn: drag = Drag::fadeIn; break;
        case Zone::fadeOut: drag = Drag::fadeOut; break;
        default: drag = Drag::move; break;
    }

    if (event.mods.isShiftDown())
    {
        if (isSelected(dragClip)) selection.removeFirstMatchingValue(dragClip);
        else selection.add(dragClip);
    }
    else if (! isSelected(dragClip))
        selection = { dragClip };
    if (drag != Drag::move)
        selection = { dragClip };

    origins.clear();
    for (const auto& id : selection)
    {
        int track = -1;
        if (const auto* clip = state.findClip(id, &track))
            origins.push_back({ id, juce::Uuid(), clip->start, track });
    }
    dragCopies = drag == Drag::move && event.mods.isCommandDown();
    repaint();
}

void TimelineComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (drag == Drag::none)
        return;
    if (! dragMoved && event.position.getDistanceFrom(mouseDownPosition) < 3.0f)
        return;
    const auto at = xToSample(event.position.x);
    const auto& state = controller.state();

    switch (drag)
    {
        case Drag::scrub:
            controller.locate(snap(at, event.mods));
            dragMoved = true;
            return;
        case Drag::loopCreate:
        {
            const auto end = snap(at, event.mods);
            controller.setLoop(true, std::min(dragAnchorSample, end), std::max(dragAnchorSample, end));
            dragMoved = true;
            return;
        }
        case Drag::loopMove:
        {
            const auto length = loopDragEnd - loopDragStart;
            const auto start = snap(std::max<juce::int64>(0, loopDragStart + at - dragAnchorSample), event.mods);
            controller.setLoop(state.loopEnabled, start, start + length);
            dragMoved = true;
            return;
        }
        case Drag::marquee:
        {
            dragMoved = true;
            marquee = juce::Rectangle<float>(mouseDownPosition, event.position);
            if (! event.mods.isShiftDown())
                selection.clear();
            for (size_t index = 0; index < state.tracks.size(); ++index)
                for (const auto& clip : state.tracks[index].clips)
                    if (clipBounds(clip, static_cast<int>(index)).intersects(marquee))
                        selection.addIfNotAlreadyThere(clip.id);
            repaint();
            return;
        }
        default: break;
    }

    if (! dragMoved)
    {
        controller.beginGesture();
        dragMoved = true;
        if (dragCopies)
            for (auto& origin : origins)
                origin.copyId = juce::Uuid();
    }

    const auto id = dragClip;
    const auto mods = event.mods;
    switch (drag)
    {
        case Drag::move:
        {
            const auto newStart = snap(dragClipStart + (at - dragAnchorSample), mods);
            auto delta = newStart - dragClipStart;
            auto trackDelta = juce::jlimit(-dragAnchorTrack, static_cast<int>(state.tracks.size()) - 1 - dragAnchorTrack,
                                           trackAt(static_cast<int>(event.position.y)) - dragAnchorTrack);
            for (const auto& origin : origins)
            {
                delta = std::max(delta, -origin.start);
                trackDelta = juce::jlimit(-origin.track, static_cast<int>(state.tracks.size()) - 1 - origin.track, trackDelta);
            }
            const auto copies = dragCopies;
            const auto moves = origins;
            controller.updateGesture([&](daw::ArrangementState& s)
            {
                if (copies)
                {
                    for (const auto& origin : moves)
                        if (const auto* clip = s.findClip(origin.id))
                        {
                            auto copy = *clip;
                            copy.id = origin.copyId;
                            copy.start = origin.start + delta;
                            daw::edit::place(s, std::move(copy), origin.track + trackDelta);
                        }
                    return;
                }
                std::vector<daw::edit::MoveRequest> requests;
                for (const auto& origin : moves)
                    requests.push_back({ origin.id, origin.start + delta, origin.track + trackDelta });
                daw::edit::moveMany(s, requests);
            });
            if (copies)
            {
                selection.clear();
                for (const auto& origin : moves)
                    selection.add(origin.copyId);
            }
            break;
        }
        case Drag::trimStart: controller.updateGesture([&](daw::ArrangementState& s) { daw::edit::trimStart(s, id, snap(at, mods)); }); break;
        case Drag::trimEnd: controller.updateGesture([&](daw::ArrangementState& s) { daw::edit::trimEnd(s, id, snap(at, mods)); }); break;
        case Drag::stretchStart: controller.updateGesture([&](daw::ArrangementState& s) { daw::edit::stretchStart(s, id, snap(at, mods)); }); break;
        case Drag::stretchEnd: controller.updateGesture([&](daw::ArrangementState& s) { daw::edit::stretchEnd(s, id, snap(at, mods)); }); break;
        case Drag::fadeIn:
        case Drag::fadeOut:
        {
            const auto fadeIn = drag == Drag::fadeIn;
            controller.updateGesture([&](daw::ArrangementState& s)
            {
                if (const auto* clip = s.findClip(id))
                    daw::edit::setFades(s, id, fadeIn ? at - clip->start : clip->fadeIn, fadeIn ? clip->fadeOut : clip->end() - at);
            });
            break;
        }
        default: break;
    }
}

void TimelineComponent::mouseUp(const juce::MouseEvent& event)
{
    // Coalesced mouse events can skip the last drag position: apply it now.
    if (drag != Drag::none && drag != Drag::marquee && dragMoved)
        mouseDrag(event);
    const auto finished = drag;
    drag = Drag::none;
    if (! dragMoved)
    {
        if (finished == Drag::marquee)
            controller.locate(snap(xToSample(event.position.x), event.mods)); // click on empty space sets the insert marker
        if (finished == Drag::loopCreate || finished == Drag::loopMove)
        {
            // A click on the loop strip toggles the loop.
            const auto& state = controller.state();
            if (state.loopEnd > state.loopStart)
                controller.setLoop(! state.loopEnabled, state.loopStart, state.loopEnd);
        }
        repaint();
        if (onSelectionChanged) onSelectionChanged();
        return;
    }

    switch (finished)
    {
        case Drag::move: controller.endGesture(dragCopies ? "Kopieren" : "Verschieben"); break;
        case Drag::trimStart:
        case Drag::trimEnd: controller.endGesture(text("Clip kürzen")); break;
        case Drag::stretchStart:
        case Drag::stretchEnd: controller.endGesture("Time-Stretch"); break;
        case Drag::fadeIn:
        case Drag::fadeOut: controller.endGesture("Fade"); break;
        default: break;
    }
    marquee = {};
    repaint();
    if (onSelectionChanged) onSelectionChanged();
}

void TimelineComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.y < rulerHeight)
        return;
    const auto hit = hitTest(event.position);
    if (hit.clip != nullptr)
        showClipProperties(hit.clip->id);
    else if (hit.track >= static_cast<int>(controller.state().tracks.size()))
        controller.addTrack({}, -1);
}

void TimelineComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown())
    {
        // Ctrl + wheel: horizontal zoom around the mouse position.
        const auto amount = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
        zoomBy(std::exp(-amount * 2.5), event.position.x);
    }
    else if (event.mods.isAltDown())
        setTrackHeight(laneHeight + static_cast<int>(std::round(wheel.deltaY * 120.0f)));
    else if (event.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
    {
        const auto amount = std::abs(wheel.deltaX) > std::abs(wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
        setViewStart(firstSample - static_cast<double>(amount) * getWidth() * 0.5 * zoom);
    }
    else
        setScrollY(verticalOffset - static_cast<int>(std::round(wheel.deltaY * 240.0f)));
}

void TimelineComponent::mouseMagnify(const juce::MouseEvent& event, float scaleFactor)
{
    if (scaleFactor > 0.0f)
        zoomBy(1.0 / scaleFactor, event.position.x);
}

void TimelineComponent::fileDragMove(const juce::StringArray&, int x, int y)
{
    dropX = static_cast<float>(x);
    dropTrack = trackAt(y);
    repaint();
}

void TimelineComponent::fileDragExit(const juce::StringArray&)
{
    dropTrack = -2;
    repaint();
}

void TimelineComponent::filesDropped(const juce::StringArray& paths, int x, int y)
{
    dropTrack = -2;
    juce::Array<juce::File> files;
    for (const auto& path : paths)
        if (juce::File(path).existsAsFile())
            files.add(juce::File(path));
    const auto track = trackAt(y);
    controller.importFiles(files, track < 0 ? currentTrack : track, snap(xToSample(static_cast<float>(x)), juce::ModifierKeys::getCurrentModifiers()));
    repaint();
}

//==============================================================================
void TimelineComponent::showClipMenu(const daw::Clip& clip, juce::int64 at)
{
    const auto id = clip.id;
    juce::PopupMenu stretch;
    for (const auto ratio : { 0.5, 0.75, 0.9, 1.0, 1.1, 1.25, 1.5, 2.0 })
        stretch.addItem(percent(ratio), [this, ratio] { stretchSelection(ratio); });
    stretch.addSeparator();
    stretch.addItem(text("An ganze Takte anpassen"), [this, id]
    {
        const auto* c = controller.state().findClip(id);
        if (c == nullptr)
            return;
        const auto bar = controller.grid().samplesPerBar();
        const auto bars = std::max(1.0, std::round(static_cast<double>(c->length) / bar));
        stretchSelection(c->stretchRatio * bars * bar / static_cast<double>(c->length));
    });

    juce::PopupMenu pitch;
    for (const auto semitones : { -12.0, -5.0, -2.0, -1.0, 1.0, 2.0, 5.0, 12.0 })
        pitch.addItem((semitones > 0 ? "+" : "") + juce::String(semitones, 0) + text(" Halbtöne"), [this, semitones] { transposeSelection(semitones, true); });
    pitch.addSeparator();
    pitch.addItem(text("Zurücksetzen"), [this] { transposeSelection(0.0, false); });

    juce::PopupMenu menu;
    menu.addItem(text("Hier teilen"), [this, id, at] { controller.perform("Teilen", [&](daw::ArrangementState& s) { daw::edit::split(s, id, at); }); });
    menu.addItem("Duplizieren\tCtrl+D", [this] { duplicateSelection(); });
    menu.addItem("Kopieren\tCtrl+C", [this] { copySelection(); });
    menu.addItem("Ausschneiden\tCtrl+X", [this] { cutSelection(); });
    menu.addItem(text("Löschen\tEntf"), [this] { deleteSelection(); });
    menu.addSeparator();
    menu.addSubMenu("Time-Stretch (Rubber Band)", stretch);
    menu.addSubMenu("Transponieren (Rubber Band)", pitch);
    menu.addItem("Fades entfernen", [this]
    {
        const auto ids = selection;
        controller.perform("Fades entfernen", [&](daw::ArrangementState& s) { for (const auto& c : ids) daw::edit::setFades(s, c, 0, 0); });
    });
    menu.addSeparator();
    menu.addItem(text("Eigenschaften …"), [this, id] { showClipProperties(id); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TimelineComponent::showLaneMenu(int track, juce::int64 at)
{
    juce::PopupMenu menu;
    menu.addItem(text("Einfügen\tCtrl+V"), ! clipboard().empty(), false, [this, at]
    {
        controller.locate(at);
        paste();
    });
    menu.addItem("Neue Spur (Rig)", [this] { controller.addTrack({}, -1); });
    menu.addItem("Neue Spur (Eingang 1, dry)", [this] { controller.addTrack({}, 0); });
    menu.addItem(text("Abspielposition hierher"), [this, at] { controller.locate(at); });
    if (track >= 0 && track < static_cast<int>(controller.state().tracks.size()))
        menu.addItem("Loop auf diesen Takt", [this, at]
        {
            const auto bar = static_cast<juce::int64>(controller.grid().samplesPerBar());
            const auto start = controller.grid().snap(at - bar / 2, 1.0);
            controller.setLoop(true, start, start + bar);
        });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TimelineComponent::showClipProperties(const juce::Uuid& id)
{
    const auto* clip = controller.state().findClip(id);
    if (clip == nullptr)
        return;
    auto* window = new juce::AlertWindow("Clip-Eigenschaften", clip->file.getFileName(), juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", clip->name, "Name");
    window->addTextEditor("gain", juce::String(clip->gainDb, 1), "Gain (dB)");
    window->addTextEditor("stretch", juce::String(clip->stretchRatio * 100.0, 2), text("Time-Stretch (%) – Rubber Band"));
    window->addTextEditor("pitch", juce::String(clip->pitchSemitones, 2), text("Transponieren (Halbtöne)"));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Abbrechen", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create([this, safe = juce::Component::SafePointer<TimelineComponent>(this), window, id](int result)
    {
        if (safe == nullptr || result != 1)
            return;
        const auto name = window->getTextEditorContents("name").trim();
        const auto gain = juce::jlimit(-48.0f, 24.0f, window->getTextEditorContents("gain").getFloatValue());
        const auto ratio = window->getTextEditorContents("stretch").getDoubleValue() / 100.0;
        const auto pitch = window->getTextEditorContents("pitch").getDoubleValue();
        controller.perform("Clip-Eigenschaften", [&](daw::ArrangementState& s)
        {
            if (auto* c = s.findClip(id))
            {
                if (name.isNotEmpty())
                    c->name = name;
                c->gainDb = gain;
            }
            if (ratio > 0.0)
                daw::edit::setStretchRatio(s, id, ratio);
            daw::edit::setPitch(s, id, pitch);
        });
    }), true);
}
} // namespace nam::ui
