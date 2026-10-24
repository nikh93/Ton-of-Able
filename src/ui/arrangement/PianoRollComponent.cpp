#include "PianoRollComponent.h"

#include <cmath>

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using daw::text;
using Theme = AmberTheme;

namespace
{
constexpr int toolbarHeight = 40;
constexpr int keyboardWidth = 64;
constexpr int velocityHeight = 72;
constexpr int scrollThickness = 10;
constexpr float edgeGrab = 6.0f;
const double gridChoices[] { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 }; // beats
const char* const gridNames[] { "1/4", "1/8", "1/16", "1/32", "1/8 Triole", "1/16 Triole" };

bool isBlackKey(int pitch)
{
    const auto inOctave = pitch % 12;
    return inOctave == 1 || inOctave == 3 || inOctave == 6 || inOctave == 8 || inOctave == 10;
}

juce::String pitchName(int pitch)
{
    static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[pitch % 12]) + juce::String(pitch / 12 - 1);
}

// Sorts notes by start (as the arrangement stores them) and returns where the flagged ones ended up.
std::set<int> sortKeepingSelection(daw::MidiNotes& notes, const std::vector<bool>& flags)
{
    std::vector<size_t> order(notes.size());
    for (size_t index = 0; index < order.size(); ++index)
        order[index] = index;
    std::stable_sort(order.begin(), order.end(), [&notes](size_t a, size_t b) { return notes[a].start < notes[b].start; });
    daw::MidiNotes sorted;
    std::set<int> selected;
    for (const auto index : order)
    {
        if (index < flags.size() && flags[index])
            selected.insert(static_cast<int>(sorted.size()));
        sorted.push_back(notes[index]);
    }
    notes = std::move(sorted);
    return selected;
}
} // namespace

PianoRollComponent::PianoRollComponent(daw::ArrangementController& c, const juce::Uuid& id, Host h)
    : controller(c), clipId(id), host(std::move(h))
{
    if (const auto* track = trackData())
        trackId = track->id;
    title.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, Theme::text());
    hint.setFont(juce::FontOptions(11.5f));
    hint.setColour(juce::Label::textColourId, Theme::subduedText());
    hint.setText(text("Klick: Note · Ziehen: Länge/Verschieben · Rand: Länge · Doppelklick: löschen · Shift+Ziehen: Auswahl · "
                      "↑↓ Halbton (Shift: Oktave) · ←→ Raster · Strg+D duplizieren · Q quantisieren"),
                 juce::dontSendNotification);
    for (size_t index = 0; index < std::size(gridNames); ++index)
        gridChoice.addItem(gridNames[index], static_cast<int>(index) + 1);
    gridChoice.setSelectedId(3, juce::dontSendNotification); // 1/16
    gridChoice.setTooltip(text("Raster für Zeichnen, Verschieben und Quantisieren (Alt: frei)"));
    gridChoice.onChange = [this] { lastLength = std::max(lastLength, gridBeats()); repaint(); };
    instrumentButton.setTooltip(text("Instrument der Spur wählen"));
    instrumentButton.onClick = [this] { if (host.showInstrumentMenu) host.showInstrumentMenu(trackId, instrumentButton); };
    quantizeButton.setButtonText(text("Quantisieren"));
    quantizeButton.setTooltip(text("Ausgewählte Noten (ohne Auswahl: alle) aufs Raster setzen (Q)"));
    quantizeButton.onClick = [this] { quantizeSelected(); };
    zoomInButton.onClick = [this] { pixelsPerBeat = std::min(800.0, pixelsPerBeat * 1.5); updateScrollBars(); repaint(); };
    zoomOutButton.onClick = [this] { pixelsPerBeat = std::max(8.0, pixelsPerBeat / 1.5); updateScrollBars(); repaint(); };
    for (juce::Component* child : std::initializer_list<juce::Component*> { &title, &hint, &gridChoice, &instrumentButton, &quantizeButton,
                                                                            &zoomInButton, &zoomOutButton, &horizontal, &vertical })
        addAndMakeVisible(*child);
    for (auto* button : { &instrumentButton, &quantizeButton, &zoomInButton, &zoomOutButton })
        button->setWantsKeyboardFocus(false);
    horizontal.addListener(this);
    vertical.addListener(this);
    horizontal.setAutoHide(false);
    vertical.setAutoHide(false);
    setWantsKeyboardFocus(true);

    // Start with the notes in view (or around middle C).
    const auto existing = notes();
    if (! existing.empty())
    {
        int highest = 0;
        for (const auto& note : existing)
            highest = std::max(highest, note.pitch);
        topPitch = static_cast<float>(std::min(127, highest + 8));
    }
    firstBeat = firstClipBeat();
    setSize(1100, 620);
    startTimerHz(30);
}

PianoRollComponent::~PianoRollComponent()
{
    preview(-1, 0);
}

//==============================================================================
const daw::Clip* PianoRollComponent::clipData() const { return controller.state().findClip(clipId); }

const daw::Track* PianoRollComponent::trackData() const
{
    int index = -1;
    if (controller.state().findClip(clipId, &index) == nullptr || index < 0)
        return nullptr;
    return &controller.state().tracks[static_cast<size_t>(index)];
}

daw::MidiNotes PianoRollComponent::notes() const
{
    const auto* clip = clipData();
    return clip != nullptr && clip->notes != nullptr ? *clip->notes : daw::MidiNotes {};
}

double PianoRollComponent::samplesPerBeat() const { return controller.grid().samplesPerBeat(); }
double PianoRollComponent::firstClipBeat() const
{
    const auto* clip = clipData();
    return clip != nullptr ? clip->offset / samplesPerBeat() : 0.0;
}
double PianoRollComponent::clipBeats() const
{
    const auto* clip = clipData();
    return clip != nullptr ? static_cast<double>(clip->length) / samplesPerBeat() : 4.0;
}
double PianoRollComponent::gridBeats() const { return gridChoices[juce::jlimit(0, static_cast<int>(std::size(gridChoices)) - 1, gridChoice.getSelectedId() - 1)]; }

double PianoRollComponent::snapBeat(double beat, const juce::ModifierKeys& mods) const
{
    if (mods.isAltDown())
        return beat;
    const auto grid = gridBeats();
    return std::round(beat / grid) * grid;
}

//==============================================================================
juce::Rectangle<int> PianoRollComponent::gridArea() const
{
    return getLocalBounds().withTrimmedTop(toolbarHeight).withTrimmedLeft(keyboardWidth)
        .withTrimmedBottom(velocityHeight + scrollThickness).withTrimmedRight(scrollThickness);
}
juce::Rectangle<int> PianoRollComponent::keyboardArea() const
{
    const auto grid = gridArea();
    return { 0, grid.getY(), keyboardWidth, grid.getHeight() };
}
juce::Rectangle<int> PianoRollComponent::velocityArea() const
{
    const auto grid = gridArea();
    return { grid.getX(), grid.getBottom() + scrollThickness, grid.getWidth(), velocityHeight };
}
float PianoRollComponent::beatToX(double beat) const { return static_cast<float>(gridArea().getX() + (beat - firstBeat) * pixelsPerBeat); }
double PianoRollComponent::xToBeat(float x) const { return firstBeat + (x - static_cast<float>(gridArea().getX())) / pixelsPerBeat; }
float PianoRollComponent::pitchToY(int pitch) const { return static_cast<float>(gridArea().getY()) + (topPitch - static_cast<float>(pitch)) * rowHeight; }
int PianoRollComponent::yToPitch(float y) const
{
    return juce::jlimit(0, 127, static_cast<int>(std::ceil(topPitch - (y - static_cast<float>(gridArea().getY())) / rowHeight)));
}
juce::Rectangle<float> PianoRollComponent::noteBounds(const daw::MidiNote& note) const
{
    const auto x0 = beatToX(note.start), x1 = beatToX(note.end());
    return { x0, pitchToY(note.pitch), std::max(3.0f, x1 - x0), rowHeight };
}

int PianoRollComponent::noteAt(juce::Point<float> point, bool& edge) const
{
    const auto list = notes();
    // Topmost (last drawn) first.
    for (int index = static_cast<int>(list.size()) - 1; index >= 0; --index)
    {
        const auto bounds = noteBounds(list[static_cast<size_t>(index)]);
        if (bounds.contains(point))
        {
            edge = point.x > bounds.getRight() - std::min(edgeGrab, bounds.getWidth() / 3.0f);
            return index;
        }
    }
    edge = false;
    return -1;
}

//==============================================================================
void PianoRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(Theme::surface());
    const auto* clip = clipData();
    const auto grid = gridArea();
    if (clip == nullptr)
    {
        g.setColour(Theme::subduedText());
        g.drawText(text("Der Clip existiert nicht mehr."), grid, juce::Justification::centred);
        return;
    }
    const auto colour = trackData() != nullptr ? trackData()->colour : Theme::amber();

    // Rows: black keys darker, C rows marked.
    {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(grid);
        g.fillAll(Theme::well());
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const auto y = pitchToY(pitch);
            if (y > static_cast<float>(grid.getBottom()) || y + rowHeight < static_cast<float>(grid.getY()))
                continue;
            if (isBlackKey(pitch))
            {
                g.setColour(juce::Colours::black.withAlpha(0.22f));
                g.fillRect(juce::Rectangle<float>(static_cast<float>(grid.getX()), y, static_cast<float>(grid.getWidth()), rowHeight));
            }
            g.setColour(juce::Colours::white.withAlpha(pitch % 12 == 0 ? 0.10f : 0.03f));
            g.drawHorizontalLine(static_cast<int>(y + rowHeight), static_cast<float>(grid.getX()), static_cast<float>(grid.getRight()));
        }
        // Beats and bars; the step grid where it is wide enough.
        const auto beatsPerBar = static_cast<double>(std::max(1, controller.state().beatsPerBar));
        const auto step = gridBeats();
        const auto from = std::floor(xToBeat(static_cast<float>(grid.getX())) / step) * step;
        for (auto beat = from; beatToX(beat) < static_cast<float>(grid.getRight()); beat += step)
        {
            const auto x = beatToX(beat);
            const auto relative = beat - firstClipBeat();
            const auto isBar = std::abs(std::fmod(relative + 1.0e-6, beatsPerBar)) < 1.0e-4;
            const auto isBeat = std::abs(relative - std::round(relative)) < 1.0e-6;
            if (! isBeat && step * pixelsPerBeat < 6.0)
                continue;
            g.setColour(juce::Colours::white.withAlpha(isBar ? 0.18f : isBeat ? 0.08f : 0.035f));
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(grid.getY()), static_cast<float>(grid.getBottom()));
        }
        // Outside the clip: shaded (notes there do not play).
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        const auto clipStartX = beatToX(firstClipBeat()), clipEndX = beatToX(firstClipBeat() + clipBeats());
        g.fillRect(juce::Rectangle<float>(static_cast<float>(grid.getX()), static_cast<float>(grid.getY()),
                                          std::max(0.0f, clipStartX - static_cast<float>(grid.getX())), static_cast<float>(grid.getHeight())));
        g.fillRect(juce::Rectangle<float>(clipEndX, static_cast<float>(grid.getY()),
                                          std::max(0.0f, static_cast<float>(grid.getRight()) - clipEndX), static_cast<float>(grid.getHeight())));

        // Notes.
        const auto list = notes();
        for (size_t index = 0; index < list.size(); ++index)
        {
            const auto& note = list[index];
            const auto bounds = noteBounds(note).reduced(0.0f, 1.0f);
            const auto selected = selection.count(static_cast<int>(index)) > 0;
            const auto shade = 0.45f + 0.55f * static_cast<float>(note.velocity) / 127.0f;
            g.setColour((selected ? Theme::amberBright() : colour).withMultipliedBrightness(shade));
            g.fillRoundedRectangle(bounds, 2.5f);
            g.setColour(selected ? juce::Colours::white : juce::Colours::black.withAlpha(0.5f));
            g.drawRoundedRectangle(bounds, 2.5f, selected ? 1.5f : 1.0f);
            if (bounds.getWidth() > 34.0f && rowHeight >= 11.0f)
            {
                g.setColour(juce::Colours::black.withAlpha(0.75f));
                g.setFont(juce::FontOptions(10.0f));
                g.drawText(pitchName(note.pitch), bounds.reduced(3.0f, 0.0f), juce::Justification::centredLeft, false);
            }
        }
        if (! marquee.isEmpty())
        {
            g.setColour(Theme::amber().withAlpha(0.15f));
            g.fillRect(marquee);
            g.setColour(Theme::amber());
            g.drawRect(marquee, 1.0f);
        }
        // Playhead inside the clip.
        const auto position = controller.engine().position();
        if (position >= clip->start && position < clip->end())
        {
            const auto x = beatToX(firstClipBeat() + static_cast<double>(position - clip->start) / samplesPerBeat());
            g.setColour(Theme::amberBright());
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(grid.getY()), static_cast<float>(grid.getBottom()));
        }
    }

    // Keyboard.
    {
        const auto keys = keyboardArea();
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(keys);
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            const auto y = pitchToY(pitch);
            if (y > static_cast<float>(keys.getBottom()) || y + rowHeight < static_cast<float>(keys.getY()))
                continue;
            const auto row = juce::Rectangle<float>(static_cast<float>(keys.getX()), y, static_cast<float>(keys.getWidth()), rowHeight);
            const auto black = isBlackKey(pitch);
            g.setColour(pitch == previewPitch ? Theme::amber() : black ? juce::Colour(0xff2a2420) : juce::Colour(0xffe9e1d4));
            g.fillRect(black ? row.withTrimmedRight(18.0f) : row);
            if (black)
            {
                g.setColour(juce::Colour(0xffe9e1d4));
                g.fillRect(row.withTrimmedLeft(row.getWidth() - 18.0f));
            }
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawHorizontalLine(static_cast<int>(y + rowHeight), row.getX(), row.getRight());
            if (pitch % 12 == 0)
            {
                g.setColour(juce::Colour(0xff40352a));
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText(pitchName(pitch), row.withTrimmedRight(4.0f).toNearestInt(), juce::Justification::centredRight, false);
            }
        }
    }

    // Velocity lane.
    {
        const auto lane = velocityArea();
        g.setColour(Theme::panel());
        g.fillRect(lane);
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawFittedText(text("ANSCHLAG"), juce::Rectangle<int>(0, lane.getY(), keyboardWidth, lane.getHeight()).reduced(4, 0),
                         juce::Justification::centred, 1, 0.8f);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(lane);
        const auto list = notes();
        for (size_t index = 0; index < list.size(); ++index)
        {
            const auto& note = list[index];
            const auto x = beatToX(note.start);
            const auto height = static_cast<float>(lane.getHeight() - 6) * static_cast<float>(note.velocity) / 127.0f;
            const auto selected = selection.count(static_cast<int>(index)) > 0;
            g.setColour(selected ? Theme::amberBright() : colour);
            g.fillRect(juce::Rectangle<float>(x, static_cast<float>(lane.getBottom()) - height, 3.0f, height));
            g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ x + 1.5f, static_cast<float>(lane.getBottom()) - height }));
        }
    }
}

void PianoRollComponent::resized()
{
    auto bar = getLocalBounds().removeFromTop(toolbarHeight).reduced(10, 6);
    zoomInButton.setBounds(bar.removeFromRight(28));
    bar.removeFromRight(4);
    zoomOutButton.setBounds(bar.removeFromRight(28));
    bar.removeFromRight(10);
    quantizeButton.setBounds(bar.removeFromRight(110));
    bar.removeFromRight(6);
    gridChoice.setBounds(bar.removeFromRight(120));
    bar.removeFromRight(6);
    instrumentButton.setBounds(bar.removeFromRight(190));
    bar.removeFromRight(10);
    title.setBounds(bar.removeFromLeft(std::min(300, bar.getWidth() / 3)));
    hint.setBounds(bar);
    const auto grid = gridArea();
    horizontal.setBounds(grid.getX(), grid.getBottom(), grid.getWidth(), scrollThickness);
    vertical.setBounds(grid.getRight(), grid.getY(), scrollThickness, grid.getHeight());
    updateScrollBars();
}

void PianoRollComponent::updateScrollBars()
{
    const auto grid = gridArea();
    const auto visibleBeats = grid.getWidth() / pixelsPerBeat;
    const auto first = firstClipBeat();
    const auto total = std::max(clipBeats() + 8.0, firstBeat - first + visibleBeats);
    horizontal.setRangeLimits(first - 4.0, first + total);
    horizontal.setCurrentRange(firstBeat, visibleBeats, juce::dontSendNotification);
    const auto visibleRows = grid.getHeight() / rowHeight;
    vertical.setRangeLimits(0.0, 128.0);
    vertical.setCurrentRange(127.0 - topPitch, visibleRows, juce::dontSendNotification);
}

void PianoRollComponent::scrollBarMoved(juce::ScrollBar* bar, double start)
{
    if (bar == &horizontal)
        firstBeat = start;
    else
        topPitch = static_cast<float>(127.0 - start);
    repaint();
}

void PianoRollComponent::timerCallback()
{
    if (const auto* track = trackData())
    {
        const auto* clip = clipData();
        title.setText(track->name + text(" · ") + (clip != nullptr ? clip->name : juce::String()), juce::dontSendNotification);
        const auto instrumentName = track->instrument->instrument != nullptr ? track->instrument->instrument->name() : juce::String("Instrument");
        if (instrumentButton.getButtonText() != instrumentName + text(" ▾"))
            instrumentButton.setButtonText(instrumentName + text(" ▾"));
    }
    if (controller.engine().isPlaying())
        repaint(gridArea());
}

//==============================================================================
void PianoRollComponent::commit(daw::MidiNotes list, const juce::String& description)
{
    std::vector<bool> flags(list.size(), false);
    for (const auto index : selection)
        if (juce::isPositiveAndBelow(index, static_cast<int>(flags.size())))
            flags[static_cast<size_t>(index)] = true;
    selection = sortKeepingSelection(list, flags);
    controller.setClipNotes(clipId, std::move(list), description);
    repaint();
}

void PianoRollComponent::updateDrag(daw::MidiNotes list)
{
    std::vector<bool> flags(list.size(), false);
    for (const auto index : dragSelection)
        if (juce::isPositiveAndBelow(index, static_cast<int>(flags.size())))
            flags[static_cast<size_t>(index)] = true;
    selection = sortKeepingSelection(list, flags);
    controller.updateGesture([this, list](daw::ArrangementState& state) { daw::edit::setNotes(state, clipId, list); });
    repaint();
}

void PianoRollComponent::deleteSelected()
{
    if (selection.empty())
        return;
    daw::MidiNotes kept;
    const auto list = notes();
    for (size_t index = 0; index < list.size(); ++index)
        if (selection.count(static_cast<int>(index)) == 0)
            kept.push_back(list[index]);
    selection.clear();
    commit(std::move(kept), text("Noten löschen"));
}

void PianoRollComponent::quantizeSelected()
{
    auto list = notes();
    const auto grid = gridBeats();
    for (size_t index = 0; index < list.size(); ++index)
        if (selection.empty() || selection.count(static_cast<int>(index)) > 0)
        {
            auto& note = list[index];
            const auto end = std::max(std::round(note.end() / grid) * grid, std::round(note.start / grid) * grid + grid);
            note.start = std::round(note.start / grid) * grid;
            note.length = end - note.start;
        }
    commit(std::move(list), text("Quantisieren"));
}

void PianoRollComponent::transposeSelected(int semitones)
{
    if (selection.empty())
        return;
    auto list = notes();
    for (const auto index : selection)
        list[static_cast<size_t>(index)].pitch = juce::jlimit(0, 127, list[static_cast<size_t>(index)].pitch + semitones);
    commit(std::move(list), text("Noten transponieren"));
    if (selection.size() == 1)
        preview(notes()[static_cast<size_t>(*selection.begin())].pitch, 100);
}

void PianoRollComponent::moveSelected(double beats)
{
    if (selection.empty())
        return;
    auto list = notes();
    for (const auto index : selection)
        list[static_cast<size_t>(index)].start = std::max(0.0, list[static_cast<size_t>(index)].start + beats);
    commit(std::move(list), text("Noten verschieben"));
}

void PianoRollComponent::duplicateSelected()
{
    if (selection.empty())
        return;
    auto list = notes();
    double from = 1.0e12, to = 0.0;
    for (const auto index : selection)
    {
        from = std::min(from, list[static_cast<size_t>(index)].start);
        to = std::max(to, list[static_cast<size_t>(index)].end());
    }
    const auto grid = gridBeats();
    const auto shift = std::max(grid, std::ceil((to - from) / grid - 1.0e-9) * grid);
    std::set<int> copies;
    for (const auto index : selection)
    {
        auto copy = list[static_cast<size_t>(index)];
        copy.start += shift;
        copies.insert(static_cast<int>(list.size()));
        list.push_back(copy);
    }
    selection = copies; // the copies stay selected for the next Ctrl+D
    commit(std::move(list), text("Noten duplizieren"));
}

void PianoRollComponent::selectAll()
{
    selection.clear();
    for (int index = 0; index < static_cast<int>(notes().size()); ++index)
        selection.insert(index);
    repaint();
}

void PianoRollComponent::preview(int pitch, int velocity)
{
    if (previewPitch >= 0)
        controller.previewNote(trackId, previewPitch, 0);
    previewPitch = velocity > 0 ? pitch : -1;
    if (previewPitch >= 0)
        controller.previewNote(trackId, pitch, velocity);
    repaint(keyboardArea());
}

//==============================================================================
void PianoRollComponent::mouseMove(const juce::MouseEvent& event)
{
    bool edge = false;
    if (gridArea().contains(event.getPosition()) && noteAt(event.position, edge) >= 0)
        setMouseCursor(edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(velocityArea().contains(event.getPosition()) ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    downPosition = event.position;
    drag = Drag::none;
    marquee = {};
    const auto point = event.getPosition();

    if (keyboardArea().contains(point))
    {
        drag = Drag::keyboard;
        preview(yToPitch(event.position.y), 100);
        return;
    }
    if (velocityArea().contains(point))
    {
        // The nearest note left of the mouse; with it the selection if it is selected.
        const auto list = notes();
        int best = -1;
        float distance = 8.0f;
        for (size_t index = 0; index < list.size(); ++index)
        {
            const auto d = std::abs(beatToX(list[index].start) - event.position.x);
            if (d < distance)
            {
                distance = d;
                best = static_cast<int>(index);
            }
        }
        if (best < 0)
            return;
        if (selection.count(best) == 0)
            selection = { best };
        dragOrigin = list;
        dragSelection = selection;
        drag = Drag::velocity;
        controller.beginGesture();
        mouseDrag(event);
        return;
    }
    if (! gridArea().contains(point))
        return;
    if (event.mods.isPopupMenu())
    {
        bool edge = false;
        if (const auto index = noteAt(event.position, edge); index >= 0)
        {
            if (selection.count(index) == 0)
                selection = { index };
            deleteSelected();
        }
        else
            showMenu(event.position);
        return;
    }
    if (event.mods.isShiftDown())
    {
        bool edge = false;
        if (const auto index = noteAt(event.position, edge); index >= 0)
        {
            // Shift+click toggles a note in the selection.
            if (! selection.erase(index))
                selection.insert(index);
            repaint();
            return;
        }
        drag = Drag::marquee;
        return;
    }

    auto list = notes();
    bool edge = false;
    dragNote = noteAt(event.position, edge);
    if (dragNote >= 0)
    {
        if (selection.count(dragNote) == 0)
            selection = { dragNote };
        drag = edge ? Drag::resize : Drag::move;
        dragOrigin = list;
        dragSelection = selection;
        lastLength = list[static_cast<size_t>(dragNote)].length;
        preview(list[static_cast<size_t>(dragNote)].pitch, list[static_cast<size_t>(dragNote)].velocity);
        controller.beginGesture();
        return;
    }
    // Empty space: draw a new note; dragging sets its length.
    const auto beat = std::max(0.0, event.mods.isAltDown() ? xToBeat(event.position.x)
                                                           : std::floor(xToBeat(event.position.x) / gridBeats()) * gridBeats());
    daw::MidiNote note { beat, lastLength, yToPitch(event.position.y), 100 };
    list.push_back(note);
    dragNote = static_cast<int>(list.size()) - 1;
    dragSelection = { dragNote };
    dragOrigin = list;
    drag = Drag::draw;
    preview(note.pitch, note.velocity);
    controller.beginGesture();
    updateDrag(list);
}

void PianoRollComponent::mouseDrag(const juce::MouseEvent& event)
{
    switch (drag)
    {
        case Drag::keyboard:
        {
            const auto pitch = yToPitch(event.position.y);
            if (pitch != previewPitch)
                preview(pitch, 100);
            return;
        }
        case Drag::marquee:
        {
            marquee = juce::Rectangle<float>(downPosition, event.position);
            selection.clear();
            const auto list = notes();
            for (size_t index = 0; index < list.size(); ++index)
                if (marquee.intersects(noteBounds(list[index])))
                    selection.insert(static_cast<int>(index));
            repaint();
            return;
        }
        case Drag::velocity:
        {
            const auto lane = velocityArea();
            const auto velocity = juce::jlimit(1, 127, static_cast<int>(std::round(127.0f * (static_cast<float>(lane.getBottom()) - event.position.y)
                                                                                     / static_cast<float>(lane.getHeight() - 6))));
            auto list = dragOrigin;
            for (const auto index : dragSelection)
                if (juce::isPositiveAndBelow(index, static_cast<int>(list.size())))
                    list[static_cast<size_t>(index)].velocity = velocity;
            updateDrag(std::move(list));
            return;
        }
        case Drag::draw:
        case Drag::resize:
        {
            auto list = dragOrigin;
            const auto& anchor = dragOrigin[static_cast<size_t>(dragNote)];
            const auto end = std::max(anchor.start + 1.0 / 64.0, snapBeat(xToBeat(event.position.x), event.mods));
            const auto delta = drag == Drag::draw ? 0.0 : (end - anchor.end());
            for (const auto index : dragSelection)
            {
                auto& note = list[static_cast<size_t>(index)];
                note.length = drag == Drag::draw ? std::max(gridBeats() * (event.mods.isAltDown() ? 0.25 : 1.0), end - note.start)
                                                 : std::max(1.0 / 64.0, note.length + delta);
            }
            lastLength = list[static_cast<size_t>(dragNote)].length;
            updateDrag(std::move(list));
            return;
        }
        case Drag::move:
        {
            const auto& anchor = dragOrigin[static_cast<size_t>(dragNote)];
            const auto beats = snapBeat(anchor.start + xToBeat(event.position.x) - xToBeat(downPosition.x), event.mods) - anchor.start;
            const auto pitches = yToPitch(event.position.y) - yToPitch(downPosition.y);
            auto list = dragOrigin;
            for (const auto index : dragSelection)
            {
                auto& note = list[static_cast<size_t>(index)];
                note.start = std::max(0.0, note.start + beats);
                note.pitch = juce::jlimit(0, 127, note.pitch + pitches);
            }
            if (const auto pitch = list[static_cast<size_t>(dragNote)].pitch; pitch != previewPitch)
                preview(pitch, list[static_cast<size_t>(dragNote)].velocity);
            updateDrag(std::move(list));
            return;
        }
        case Drag::none: return;
    }
}

void PianoRollComponent::mouseUp(const juce::MouseEvent& event)
{
    if (drag != Drag::none && drag != Drag::keyboard && event.mouseWasDraggedSinceMouseDown())
        mouseDrag(event); // the release position counts
    preview(-1, 0);
    const auto finished = std::exchange(drag, Drag::none);
    marquee = {};
    switch (finished)
    {
        case Drag::draw: controller.endGesture(text("Note zeichnen")); break;
        case Drag::move: controller.endGesture(text("Noten verschieben")); break;
        case Drag::resize: controller.endGesture(text("Notenlänge")); break;
        case Drag::velocity: controller.endGesture(text("Anschlag")); break;
        default: break;
    }
    repaint();
}

void PianoRollComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    bool edge = false;
    if (const auto index = noteAt(event.position, edge); index >= 0)
    {
        selection = { index };
        deleteSelected();
    }
}

void PianoRollComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown())
    {
        const auto anchor = xToBeat(event.position.x);
        pixelsPerBeat = juce::jlimit(8.0, 800.0, pixelsPerBeat * std::exp(wheel.deltaY * 1.5));
        firstBeat = anchor - (event.position.x - static_cast<float>(gridArea().getX())) / pixelsPerBeat;
    }
    else if (event.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
    {
        const auto amount = std::abs(wheel.deltaX) > std::abs(wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
        firstBeat -= amount * gridArea().getWidth() * 0.5 / pixelsPerBeat;
    }
    else
        topPitch = juce::jlimit(static_cast<float>(gridArea().getHeight()) / rowHeight - 1.0f, 127.0f, topPitch + wheel.deltaY * 12.0f);
    updateScrollBars();
    repaint();
}

bool PianoRollComponent::keyPressed(const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto mods = key.getModifiers();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        deleteSelected();
    else if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
        transposeSelected((code == juce::KeyPress::upKey ? 1 : -1) * (mods.isShiftDown() ? 12 : 1));
    else if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
        moveSelected((code == juce::KeyPress::rightKey ? 1.0 : -1.0) * gridBeats());
    else if (mods.isCommandDown() && (code == 'A' || code == 'a'))
        selectAll();
    else if (mods.isCommandDown() && (code == 'D' || code == 'd'))
        duplicateSelected();
    else if (mods.isCommandDown() && (code == 'Z' || code == 'z'))
        mods.isShiftDown() ? controller.redo() : controller.undo();
    else if (mods.isCommandDown() && (code == 'Y' || code == 'y'))
        controller.redo();
    else if (! mods.isAnyModifierKeyDown() && (code == 'Q' || code == 'q'))
        quantizeSelected();
    else if (code == juce::KeyPress::escapeKey)
    {
        selection.clear();
        repaint();
    }
    else
        return false;
    return true;
}

void PianoRollComponent::showMenu(juce::Point<float>)
{
    juce::PopupMenu menu;
    menu.addItem(text("Alles auswählen\tStrg+A"), [this] { selectAll(); });
    menu.addItem(text("Quantisieren\tQ"), [this] { quantizeSelected(); });
    menu.addItem(text("Duplizieren\tStrg+D"), ! selection.empty(), false, [this] { duplicateSelected(); });
    menu.addItem(text("Löschen\tEntf"), ! selection.empty(), false, [this] { deleteSelected(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}
} // namespace nam::ui
