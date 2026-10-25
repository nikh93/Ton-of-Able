#include "ArrangementView.h"
#include "DrumMachineComponent.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"
#include "ui/shortcuts/Shortcuts.h"

namespace nam::ui
{
namespace
{
using Theme = AmberTheme;
using daw::text;

struct SnapChoice { const char* name; double bars; };
const SnapChoice snapChoices[] { { "Raster aus", 0.0 }, { "1 Takt", 1.0 }, { "1/2", 0.5 }, { "1/4", 0.25 }, { "1/8", 0.125 }, { "1/16", 0.0625 }, { "1/32", 0.03125 } };
struct SignatureChoice { int beats; int unit; };
const SignatureChoice signatures[] { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 8 }, { 7, 8 }, { 12, 8 } };

juce::String formatTime(juce::int64 samples, double rate)
{
    const auto seconds = std::max(0.0, static_cast<double>(samples) / rate);
    const auto minutes = static_cast<int>(seconds / 60.0);
    return juce::String(minutes).paddedLeft('0', 2) + ":" + juce::String(seconds - minutes * 60.0, 3).paddedLeft('0', 6);
}
} // namespace

class ArrangementView::HeaderColumn final : public juce::Component
{
public:
    explicit HeaderColumn(ArrangementView& v) : view(v)
    {
        addTrackButton.setTooltip(text("Spur hinzufügen: Audio, Drums, Eingang oder Return (Ctrl+T = Audiospur)"));
        addTrackButton.onClick = [this] { view.showAddTrackMenu(addTrackButton); };
        addAndMakeVisible(addTrackButton);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Theme::panel().darker(0.15f));
        auto top = getLocalBounds().removeFromTop(TimelineComponent::rulerHeight);
        g.setColour(Theme::panel());
        g.fillRect(top);
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawHorizontalLine(top.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("SPUREN  " + juce::String(view.controller.state().tracks.size()), top.reduced(12, 0), juce::Justification::centredLeft);
        if (view.controller.state().tracks.empty())
        {
            g.setColour(Theme::subduedText().withAlpha(0.6f));
            g.setFont(juce::FontOptions(12.0f));
            g.drawFittedText(text("Doppelklick hier oder „+ Spur“\nlegt eine Spur an."),
                             getLocalBounds().withTrimmedTop(TimelineComponent::rulerHeight + 16).reduced(12, 0).removeFromTop(40),
                             juce::Justification::centredTop, 2);
        }
    }

    void resized() override
    {
        addTrackButton.setBounds(getLocalBounds().removeFromTop(TimelineComponent::rulerHeight).removeFromRight(84).reduced(6, 4));
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        if (event.y > TimelineComponent::rulerHeight)
            view.controller.addTrack({}, -1);
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        view.timeline.mouseWheelMove(event.getEventRelativeTo(&view.timeline), wheel);
    }

private:
    ArrangementView& view;
    IconButton addTrackButton { Icon::plus, "Spur" };
};

ArrangementView::ArrangementView(daw::ArrangementController& c) : controller(c)
{
    headerColumn = std::make_unique<HeaderColumn>(*this);
    addAndMakeVisible(*headerColumn);
    addAndMakeVisible(timeline);
    addAndMakeVisible(exportRange);
    addAndMakeVisible(chordBar);
    chordBar.onAccompaniment = [this] { openAccompaniment({}); };
    chordBar.onOpenEditor = [this](int bar) { openProgressionEditor(bar); };
    for (auto* bar : { &horizontal, &vertical })
    {
        bar->setAutoHide(false);
        bar->setColour(juce::ScrollBar::thumbColourId, Theme::amber().withAlpha(0.4f));
        bar->setColour(juce::ScrollBar::trackColourId, Theme::panel().darker(0.2f));
        bar->addListener(this);
        addAndMakeVisible(*bar);
    }

    record.setIconColour(Theme::record());
    record.setColour(juce::TextButton::buttonOnColourId, Theme::record());
    record.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    play.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff6fbf73));
    play.setIconColour(juce::Colour(0xff8fd694));
    toStart.setTooltip("Zum Anfang (Pos1)");
    play.setTooltip("Abspielen / Stoppen (Leertaste)");
    stopButton.setTooltip(text("Stopp – zweimal: zurück zum Anfang"));
    record.setTooltip(text("Aufnahme auf allen scharfen Spuren (F9). Ohne scharfe Spur wird die ausgewählte Spur verwendet."));
    loop.setTooltip(text("Loop an/aus (Ctrl+L). Einschalten nimmt die ausgewählten Clips, sonst den bisherigen Bereich (wenn sichtbar), sonst 4 Takte ab dem Takt der Abspielposition. Im Lineal oben ziehen: Bereich neu, Mitte verschiebt, Enden ändern die Grenzen."));
    metronome.setTooltip("Metronom (Klick)");
    countIn.setTooltip(text("Ein Takt Vorzähler vor der Aufnahme"));
    undo.setTooltip(text("Rückgängig (Ctrl+Z)"));
    redo.setTooltip("Wiederholen (Ctrl+Y / Ctrl+Shift+Z)");
    pointerTool.setTooltip(text("Zeiger (V): verschieben, Kanten ziehen = kürzen, Ctrl+Kante = Time-Stretch, Ctrl+Ziehen = kopieren, Ecken oben = Fades, Alt = ohne Raster"));
    splitTool.setTooltip(text("Schere (C): Klick teilt den Clip"));
    stretchTool.setTooltip(text("Stretch (T): Kanten ziehen = Time-Stretch mit Rubber Band"));
    zoomIn.setTooltip("Hineinzoomen (Ctrl+Mausrad / Ctrl++)");
    zoomOut.setTooltip("Herauszoomen (Ctrl+Mausrad / Ctrl+-)");
    zoomFit.setTooltip(text("Ganzes Projekt zeigen"));
    importButton.setTooltip(text("Audio oder Video importieren (Ctrl+I) – auch per Drag & Drop auf die Timeline"));
    exportButton.setTooltip(text("Mix, Stems oder einzelne Spuren exportieren – im Bereich des Export-Balkens unten"));
    projectButton.setTooltip(text("Projekt: Neu, Öffnen, Speichern, Speichern unter …"));
    shortcutsButton.setTooltip(text("Tastenkürzel ansehen und frei belegen (F1)"));

    for (auto* button : { &toStart, &play, &stopButton, &record, &loop, &metronome, &countIn, &pointerTool, &splitTool, &stretchTool,
                          &zoomOut, &zoomIn, &zoomFit, &undo, &redo, &importButton, &exportButton, &projectButton, &shortcutsButton })
    {
        button->setWantsKeyboardFocus(false);
        addAndMakeVisible(*button);
    }
    for (auto* button : { &pointerTool, &splitTool, &stretchTool })
    {
        button->setClickingTogglesState(true);
        button->setRadioGroupId(4711);
    }
    pointerTool.setToggleState(true, juce::dontSendNotification);
    // Related buttons join into segmented groups.
    const auto group = [](std::initializer_list<juce::Button*> buttons)
    {
        const auto count = static_cast<int>(buttons.size());
        int index = 0;
        for (auto* button : buttons)
        {
            button->setConnectedEdges((index > 0 ? juce::Button::ConnectedOnLeft : 0) | (index < count - 1 ? juce::Button::ConnectedOnRight : 0));
            ++index;
        }
    };
    group({ &toStart, &play, &stopButton, &record });
    group({ &loop, &metronome, &countIn });
    group({ &pointerTool, &splitTool, &stretchTool });
    group({ &zoomOut, &zoomIn, &zoomFit });
    group({ &undo, &redo });

    toStart.onClick = [this] { controller.locate(0); };
    play.onClick = [this] { controller.togglePlay(); };
    stopButton.onClick = [this] { controller.stop(); };
    record.onClick = [this] { controller.toggleRecord(timeline.selectedTrack()); };
    loop.onClick = [this]
    {
        const auto& state = controller.state();
        if (state.loopEnabled)
        {
            controller.setLoop(false, state.loopStart, state.loopEnd); // the range stays for next time
            return;
        }
        // Switching on: the selected clips, else the previous range if it is in view,
        // else four bars from the bar of the playhead (or the first bar in view).
        auto start = state.loopStart, end = state.loopEnd;
        const auto [viewFrom, viewTo] = timeline.visibleRange();
        if (const auto selected = timeline.selectionRange())
        {
            start = selected->first;
            end = selected->second;
        }
        else if (end <= start || end <= viewFrom || start >= viewTo)
        {
            const auto bar = controller.grid().samplesPerBar();
            const auto barOf = [bar](juce::int64 at) { return static_cast<juce::int64>(std::floor(static_cast<double>(at) / bar) * bar); };
            const auto playhead = controller.engine().position();
            start = playhead >= viewFrom && playhead < viewTo ? barOf(playhead) : barOf(viewFrom + static_cast<juce::int64>(bar) - 1);
            end = start + static_cast<juce::int64>(bar * 4.0);
        }
        controller.setLoop(true, start, end);
    };
    metronome.onClick = [this] { controller.engine().setMetronome(! controller.engine().isMetronomeOn()); controller.markDirty(); updateTransportButtons(); };
    countIn.onClick = [this] { controller.setCountIn(! controller.isCountInEnabled()); controller.markDirty(); updateTransportButtons(); };
    pointerTool.onClick = [this] { timeline.setTool(TimelineComponent::Tool::pointer); };
    splitTool.onClick = [this] { timeline.setTool(TimelineComponent::Tool::split); };
    stretchTool.onClick = [this] { timeline.setTool(TimelineComponent::Tool::stretch); };
    zoomIn.onClick = [this] { timeline.zoomBy(0.5, static_cast<float>(timeline.getWidth()) * 0.5f); };
    zoomOut.onClick = [this] { timeline.zoomBy(2.0, static_cast<float>(timeline.getWidth()) * 0.5f); };
    zoomFit.onClick = [this] { timeline.zoomToFit(); };
    undo.onClick = [this] { controller.undo(); };
    redo.onClick = [this] { controller.redo(); };
    exportBar.setColour(juce::ProgressBar::foregroundColourId, Theme::amber());
    exportBar.setColour(juce::ProgressBar::backgroundColourId, Theme::well());
    cancelExportButton.setButtonText(text("Abbrechen"));
    cancelExportButton.setTooltip(text("Export abbrechen – angefangene Dateien werden gelöscht"));
    cancelExportButton.onClick = [this] { controller.cancelExport(); };
    addChildComponent(exportBar);
    addChildComponent(cancelExportButton);
    importButton.onClick = [this] { chooseImport(); };
    exportButton.onClick = [this] { showExportMenu(); };
    projectButton.onClick = [this] { showProjectMenu(); };
    shortcutsButton.onClick = [this] { if (onShowShortcuts) onShowShortcuts(); };

    position.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 19.0f, juce::Font::bold));
    position.setColour(juce::Label::textColourId, Theme::amberBright());
    position.setJustificationType(juce::Justification::centred);
    position.setTooltip("Position in Takt.Schlag.Sechzehntel");
    clock.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    clock.setColour(juce::Label::textColourId, Theme::subduedText());
    clock.setJustificationType(juce::Justification::centred);
    clock.setTooltip("Position in Minuten:Sekunden");
    for (auto* label : { &position, &clock })
    {
        label->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*label);
    }

    bpm.setEditable(false, true, false);
    bpm.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::bold));
    bpm.setColour(juce::Label::textColourId, Theme::text());
    bpm.setJustificationType(juce::Justification::centred);
    bpm.setTooltip(text("Tempo – Doppelklick zum Ändern"));
    bpm.onTextChange = [this]
    {
        const auto value = bpm.getText().getDoubleValue();
        const auto& state = controller.state();
        if (value >= 20.0 && value <= 300.0)
            controller.setTempo(value, state.beatsPerBar, state.beatUnit);
        stateChanged();
    };
    for (auto* label : { &bpmLabel, &masterLabel })
    {
        label->setColour(juce::Label::textColourId, Theme::subduedText());
        label->setFont(juce::FontOptions(10.5f, juce::Font::bold));
        label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*label);
    }
    addAndMakeVisible(bpm);

    for (size_t index = 0; index < std::size(signatures); ++index)
        signature.addItem(juce::String(signatures[index].beats) + "/" + juce::String(signatures[index].unit), static_cast<int>(index) + 1);
    signature.setTooltip("Taktart");
    signature.setColour(juce::ComboBox::backgroundColourId, Theme::well());
    signature.onChange = [this]
    {
        const auto index = signature.getSelectedItemIndex();
        if (index >= 0 && (signatures[index].beats != controller.state().beatsPerBar || signatures[index].unit != controller.state().beatUnit))
            controller.setTempo(controller.state().bpm, signatures[index].beats, signatures[index].unit);
    };
    addAndMakeVisible(signature);

    for (size_t index = 0; index < std::size(snapChoices); ++index)
        snap.addItem(snapChoices[index].name, static_cast<int>(index) + 1);
    snap.setSelectedItemIndex(3, juce::dontSendNotification);
    snap.setTooltip(text("Raster zum Einrasten (Alt beim Ziehen = frei)"));
    snap.onChange = [this] { timeline.setSnapDivision(snapChoices[std::max(0, snap.getSelectedItemIndex())].bars); timeline.repaint(); };
    addAndMakeVisible(snap);

    master.setRange(-60.0, 6.0, 0.1);
    master.setSkewFactorFromMidPoint(-12.0);
    master.setDoubleClickReturnValue(true, 0.0);
    master.setPopupDisplayEnabled(true, true, nullptr);
    master.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };
    master.setTooltip(text("Lautstärke der Arrangement-Wiedergabe (das Live-Rig regelt OUTPUT)"));
    master.onValueChange = [this] { controller.engine().setMasterGainDb(static_cast<float>(master.getValue())); controller.markDirty(); };
    addAndMakeVisible(master);


    timeline.onViewChanged = [this] { layoutHeaders(); updateScrollBars(); };
    timeline.onOpenDrums = [this](const juce::Uuid& id, int pattern) { openDrumMachine(id, pattern); };
    timeline.onOpenDrumArrangement = [this](const juce::Uuid& id, int bar) { openDrumArrangement(id, bar); };
    timeline.onOpenPianoRoll = [this](const juce::Uuid& id) { openPianoRoll(id); };
    timeline.onSelectionChanged = [this] { for (auto& header : headers) header->repaint(); stateChanged(); };

    controller.onChanged = [this] { stateChanged(); };
    controller.onMessage = [this](const juce::String& message) { status(message); };
    setWantsKeyboardFocus(true);
    stateChanged();
    startTimerHz(30);
}

namespace
{
class FxWindow final : public juce::DocumentWindow
{
public:
    explicit FxWindow(std::unique_ptr<juce::Component> editor)
        : DocumentWindow(daw::text("Spur-Effekte"), AmberTheme::surface(), closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor.release(), true);
        setResizable(true, false);
        setResizeLimits(940, 760, 1800, 1300);
        centreWithSize(getWidth(), getHeight());
    }
    void closeButtonPressed() override { setVisible(false); }
};
} // namespace

void ArrangementView::openTrackFx(const juce::Uuid& trackId)
{
    if (! createTrackFxEditor || controller.state().findTrack(trackId) == nullptr)
        return;
    // Recreate the editor each time so it reflects the current chain.
    fxWindows[trackId] = std::make_unique<FxWindow>(createTrackFxEditor(trackId));
    fxWindows[trackId]->setVisible(true);
    fxWindows[trackId]->toFront(true);
}

void ArrangementView::openPianoRoll(const juce::Uuid& clipId)
{
    const auto* clip = controller.state().findClip(clipId);
    if (clip == nullptr || ! clip->isMidi())
        return;
    if (auto existing = pianoWindows.find(clipId); existing != pianoWindows.end())
    {
        existing->second->setVisible(true);
        existing->second->toFront(true);
        return;
    }
    PianoRollComponent::Host host;
    host.showInstrumentMenu = [this](const juce::Uuid& trackId, juce::Component& target) { if (showInstrumentMenu) showInstrumentMenu(trackId, target); };
    auto window = std::make_unique<FxWindow>(std::make_unique<PianoRollComponent>(controller, clipId, std::move(host)));
    window->setName(daw::text("Piano-Roll"));
    window->setResizeLimits(700, 420, 2400, 1600);
    window->centreWithSize(1100, 620);
    window->setVisible(true);
    window->toFront(true);
    pianoWindows[clipId] = std::move(window);
}

void ArrangementView::openAccompaniment(const juce::Uuid& trackId)
{
    auto panel = std::make_unique<AccompanimentPanel>(controller, trackId, createSpaceEffect);
    panel->onDone = [this] { juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ArrangementView>(this)] { if (safe != nullptr) safe->accompanimentWindow.reset(); }); };
    const auto size = panel->getBounds();
    auto window = std::make_unique<FxWindow>(std::move(panel));
    window->setName(daw::text("Begleitung aus Akkorden"));
    window->setResizeLimits(size.getWidth(), size.getHeight(), size.getWidth(), size.getHeight());
    window->setResizable(false, false);
    window->centreWithSize(size.getWidth(), size.getHeight());
    window->setVisible(true);
    window->toFront(true);
    accompanimentWindow = std::move(window);
}

void ArrangementView::openProgressionEditor(int bar)
{
    auto editor = std::make_unique<ChordProgressionEditor>(controller, bar);
    editor->onClose = [this] { juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ArrangementView>(this)] { if (safe != nullptr) safe->progressionWindow.reset(); }); };
    const auto size = editor->getBounds();
    auto window = std::make_unique<FxWindow>(std::move(editor));
    window->setName(daw::text("Akkordfolgen"));
    window->setResizeLimits(size.getWidth(), size.getHeight(), size.getWidth() + 600, size.getHeight() + 400);
    window->centreWithSize(size.getWidth(), size.getHeight());
    window->setVisible(true);
    window->toFront(true);
    progressionWindow = std::move(window);
}

void ArrangementView::openDrumArrangement(const juce::Uuid& trackId, int bar)
{
    const auto* track = controller.state().findTrack(trackId);
    if (track == nullptr || ! track->drums)
        return;
    auto editor = std::make_unique<DrumArrangementEditor>(controller, trackId, bar);
    editor->onClose = [this] { juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ArrangementView>(this)] { if (safe != nullptr) safe->drumArrangementWindow.reset(); }); };
    const auto size = editor->getBounds();
    auto window = std::make_unique<FxWindow>(std::move(editor));
    window->setName(daw::text("Drum-Arrangement"));
    window->setResizeLimits(size.getWidth(), size.getHeight(), size.getWidth() + 600, size.getHeight() + 400);
    window->centreWithSize(size.getWidth(), size.getHeight());
    window->setVisible(true);
    window->toFront(true);
    drumArrangementWindow = std::move(window);
}

void ArrangementView::openDrumMachine(const juce::Uuid& trackId, int pattern)
{
    if (! createDrumEditor || controller.state().findTrack(trackId) == nullptr)
        return;
    if (auto existing = drumWindows.find(trackId); existing != drumWindows.end())
        if (auto* machine = dynamic_cast<DrumMachineComponent*>(existing->second->getContentComponent()))
        {
            machine->selectPattern(pattern);
            existing->second->setVisible(true);
            existing->second->toFront(true);
            return;
        }
    auto window = std::make_unique<FxWindow>(createDrumEditor(trackId, pattern));
    window->setName(daw::text("Drumcomputer"));
    window->setResizeLimits(900, 480, 1800, 1200);
    window->setVisible(true);
    window->toFront(true);
    drumWindows[trackId] = std::move(window);
}

ArrangementView::~ArrangementView()
{
    stopTimer();
    fxWindows.clear();
    drumWindows.clear();
    pianoWindows.clear();
    accompanimentWindow.reset();
    progressionWindow.reset();
    drumArrangementWindow.reset();
    controller.onChanged = nullptr;
    controller.onMessage = nullptr;
}

//==============================================================================
juce::ValueTree ArrangementView::saveState() const
{
    juce::ValueTree state("arrangementView");
    state.setProperty("project", controller.projectFile().getFullPathName(), nullptr);
    state.setProperty("snap", snap.getSelectedItemIndex(), nullptr);
    state.setProperty("zoom", timeline.samplesPerPixel(), nullptr);
    state.setProperty("trackHeight", timeline.trackHeight(), nullptr);
    state.setProperty("exportFormat", static_cast<int>(exportFormat), nullptr);
    state.setProperty("exportDirectory", lastExportDirectory.getFullPathName(), nullptr);
    state.setProperty("ffmpeg", daw::codecs::findFfmpeg().getFullPathName(), nullptr);
    return state;
}

void ArrangementView::restoreState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return;
    if (const auto ffmpeg = juce::File(state.getProperty("ffmpeg").toString()); ffmpeg.existsAsFile())
        daw::codecs::setFfmpegOverride(ffmpeg);
    snap.setSelectedItemIndex(juce::jlimit(0, static_cast<int>(std::size(snapChoices)) - 1, static_cast<int>(state.getProperty("snap", 3))), juce::sendNotificationSync);
    timeline.setTrackHeight(state.getProperty("trackHeight", 92));
    const auto zoom = static_cast<double>(state.getProperty("zoom", 400.0));
    timeline.zoomBy(zoom / timeline.samplesPerPixel(), 0.0f);
    exportFormat = static_cast<daw::codecs::ExportFormat>(static_cast<int>(state.getProperty("exportFormat", 1)));
    lastExportDirectory = juce::File(state.getProperty("exportDirectory").toString());
    if (const auto project = juce::File(state.getProperty("project").toString()); project.existsAsFile())
    {
        if (const auto result = controller.openProject(project); result.failed())
            status(result.getErrorMessage());
    }
    stateChanged();
}

void ArrangementView::autosave()
{
    const auto& state = controller.state();
    const auto hasContent = state.endSample() > 0;
    if (controller.hasUnsavedChanges() && (hasContent || controller.projectFile().existsAsFile()))
        if (const auto result = controller.save(); result.failed())
            status(result.getErrorMessage());
}

//==============================================================================
void ArrangementView::stateChanged()
{
    const auto& state = controller.state();
    timeline.stateChanged();
    rebuildHeaders();
    if (! bpm.isBeingEdited())
        bpm.setText(juce::String(state.bpm, state.bpm == std::round(state.bpm) ? 0 : 2), juce::dontSendNotification);
    for (size_t index = 0; index < std::size(signatures); ++index)
        if (signatures[index].beats == state.beatsPerBar && signatures[index].unit == state.beatUnit)
            signature.setSelectedItemIndex(static_cast<int>(index), juce::dontSendNotification);
    if (! master.isMouseButtonDown())
        master.setValue(controller.engine().masterGainDb(), juce::dontSendNotification);
    undo.setEnabled(controller.canUndo());
    redo.setEnabled(controller.canRedo());
    undo.setTooltip(controller.canUndo() ? text("Rückgängig: ") + controller.undoDescription() + " (Ctrl+Z)" : text("Rückgängig (Ctrl+Z)"));
    {
        auto name = controller.projectFile().getFileNameWithoutExtension();
        if (name.isEmpty())
            name = text("Neues Projekt");
        projectButton.setButtonText(name + (controller.hasUnsavedChanges() ? text(" \xe2\x80\xa2") : juce::String()));
        headerColumn->repaint();
    }
    updateTransportButtons();
    updateScrollBars();
}

void ArrangementView::rebuildHeaders()
{
    const auto& tracks = controller.state().tracks;
    bool same = headers.size() == tracks.size();
    for (size_t index = 0; same && index < tracks.size(); ++index)
        same = headers[index]->trackId() == tracks[index].id;
    if (! same)
    {
        // Reuse headers by track id so editors and drags survive reordering.
        std::vector<std::unique_ptr<TrackHeaderComponent>> next;
        for (const auto& track : tracks)
        {
            auto existing = std::find_if(headers.begin(), headers.end(), [&](const auto& h) { return h != nullptr && h->trackId() == track.id; });
            if (existing != headers.end())
                next.push_back(std::move(*existing));
            else
            {
                auto header = std::make_unique<TrackHeaderComponent>(controller, track.id);
                header->onSelect = [this](const juce::Uuid& id) { timeline.selectTrack(controller.state().trackIndex(id)); };
                header->onExportTrack = [this](const juce::Uuid& id) { exportTo(false, id); };
                header->onOpenFx = [this](const juce::Uuid& id) { openTrackFx(id); };
                header->onOpenDrums = [this](const juce::Uuid& id) { openDrumMachine(id, 0); };
                header->onInstrumentMenu = [this](const juce::Uuid& id, juce::Component& target) { if (showInstrumentMenu) showInstrumentMenu(id, target); };
                header->onEditAccompaniment = [this](const juce::Uuid& id) { openAccompaniment(id); };
                header->onOpenDrumArrangement = [this](const juce::Uuid& id) { openDrumArrangement(id, 0); };
                headerColumn->addAndMakeVisible(*header);
                next.push_back(std::move(header));
            }
        }
        headers = std::move(next); // headers of removed tracks are destroyed here
        for (auto* windows : { &fxWindows, &drumWindows })
            for (auto it = windows->begin(); it != windows->end();)
                it = controller.state().findTrack(it->first) == nullptr ? windows->erase(it) : std::next(it);
        // Piano rolls belong to clips.
        for (auto it = pianoWindows.begin(); it != pianoWindows.end();)
            it = controller.state().findClip(it->first) == nullptr ? pianoWindows.erase(it) : std::next(it);
    }
    const auto inputs = controller.availableInputs();
    for (size_t index = 0; index < tracks.size(); ++index)
        headers[index]->refresh(tracks[index], inputs, static_cast<int>(index) == timeline.selectedTrack());
    layoutHeaders();
}

void ArrangementView::layoutHeaders()
{
    const auto height = timeline.trackHeight();
    for (size_t index = 0; index < headers.size(); ++index)
    {
        const auto top = TimelineComponent::rulerHeight + static_cast<int>(index) * height - timeline.scrollY();
        headers[index]->setBounds(0, top, headerColumn->getWidth(), height);
        headers[index]->setVisible(top + height > TimelineComponent::rulerHeight && top < headerColumn->getHeight());
    }
}

void ArrangementView::updateScrollBars()
{
    const auto visibleSamples = timeline.getWidth() * timeline.samplesPerPixel();
    horizontal.setRangeLimits(0.0, std::max(timeline.contentLengthSamples(), timeline.viewStart() + visibleSamples), juce::dontSendNotification);
    horizontal.setCurrentRange(timeline.viewStart(), visibleSamples, juce::dontSendNotification);
    const auto visibleHeight = std::max(1, timeline.getHeight() - TimelineComponent::rulerHeight);
    vertical.setRangeLimits(0.0, std::max(timeline.contentHeight(), visibleHeight), juce::dontSendNotification);
    vertical.setCurrentRange(timeline.scrollY(), visibleHeight, juce::dontSendNotification);
}

void ArrangementView::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &horizontal)
        timeline.setViewStart(newRangeStart);
    else
        timeline.setScrollY(static_cast<int>(newRangeStart));
}

void ArrangementView::updateTransportButtons()
{
    auto& engine = controller.engine();
    play.setToggleState(engine.isPlaying(), juce::dontSendNotification);
    record.setToggleState(engine.isRecording(), juce::dontSendNotification);
    loop.setToggleState(controller.state().loopEnabled, juce::dontSendNotification);
    metronome.setToggleState(engine.isMetronomeOn(), juce::dontSendNotification);
    countIn.setToggleState(controller.isCountInEnabled(), juce::dontSendNotification);
}

void ArrangementView::timerCallback()
{
    auto& engine = controller.engine();
    const auto grid = controller.grid();
    position.setText(grid.formatPosition(engine.position()), juce::dontSendNotification);
    clock.setText(formatTime(engine.position(), grid.sampleRate), juce::dontSendNotification);
    updateTransportButtons();
    for (auto& header : headers)
        header->updateMeter();
    if (engine.isPlaying() || controller.pendingWork() > 0 || controller.isInGesture())
    {
        if (followEnabled)
            timeline.followPlayhead();
        timeline.repaint();
        updateScrollBars();
    }
    exportRange.update();
    chordBar.update();
    // Export progress replaces Import/Export while it runs.
    const auto progress = controller.exportProgress();
    const auto exportRunning = progress >= 0.0;
    if (exportBar.isVisible() != exportRunning)
    {
        exportBar.setVisible(exportRunning);
        cancelExportButton.setVisible(exportRunning);
        importButton.setVisible(! exportRunning);
        exportButton.setVisible(! exportRunning);
    }
    if (exportRunning)
    {
        const auto converting = controller.exportConverting();
        exportFraction = converting ? -1.0 : progress; // -1: the bar moves without a value
        exportBar.setTextToDisplay(converting ? text("Konvertiere …") : text("Export ") + juce::String(juce::roundToInt(progress * 100.0)) + " %");
    }
    if (engine.isRecording())
        record.setAlpha((juce::Time::getMillisecondCounter() / 400) % 2 == 0 ? 1.0f : 0.55f);
    else
        record.setAlpha(1.0f);
}

//==============================================================================
void ArrangementView::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background());
    const auto bar = getLocalBounds().removeFromTop(toolbarHeight);
    g.setColour(Theme::panel());
    g.fillRect(bar);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawHorizontalLine(bar.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));
    // Thin divider between the transport row and the edit row.
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawHorizontalLine(bar.getY() + 48, 8.0f, static_cast<float>(getWidth() - 8));

    if (! displayArea.isEmpty())
    {
        const auto display = displayArea.toFloat();
        g.setColour(Theme::well());
        g.fillRoundedRectangle(display, 7.0f);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        g.drawRoundedRectangle(display.reduced(0.5f), 7.0f, 1.0f);
        // Separators between position, tempo and time signature.
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        for (auto* component : { static_cast<juce::Component*>(&bpmLabel), static_cast<juce::Component*>(&signature) })
            g.drawVerticalLine(component->getX() - 7, display.getY() + 7.0f, display.getBottom() - 7.0f);
    }
}

void ArrangementView::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop(toolbarHeight).reduced(10, 0);
    auto row = bar.removeFromTop(48).withSizeKeepingCentre(bar.getWidth(), 36);
    const auto place = [&row](juce::Component& component, int width, int gap = 0)
    {
        component.setBounds(row.removeFromLeft(width));
        row.removeFromLeft(gap);
    };

    // Transport row: project | transport | display | loop, click, count-in ... master | shortcuts
    const auto compact = getWidth() < 1180;
    place(projectButton, compact ? 128 : 180, 14);
    place(toStart, 40);
    place(play, 46);
    place(stopButton, 40);
    place(record, 46, 14);

    displayArea = row.removeFromLeft(compact ? 330 : 370);
    row.removeFromLeft(14);
    {
        auto display = displayArea.reduced(10, 3);
        position.setBounds(display.removeFromLeft(compact ? 70 : 84));
        clock.setBounds(display.removeFromLeft(compact ? 78 : 90).translated(0, 1));
        display.removeFromLeft(14);
        auto tempo = display.removeFromLeft(compact ? 62 : 76);
        bpmLabel.setBounds(tempo.removeFromLeft(28));
        bpm.setBounds(tempo);
        display.removeFromLeft(14);
        signature.setBounds(display.withSizeKeepingCentre(display.getWidth(), 26));
    }
    place(loop, 40);
    place(metronome, 40);
    place(countIn, 40, 14);

    shortcutsButton.setBounds(row.removeFromRight(40));
    row.removeFromRight(14);
    master.setBounds(row.removeFromRight(juce::jlimit(60, 160, row.getWidth() - 52)));
    masterLabel.setBounds(row.removeFromRight(52));

    // Edit row: tools | snap | zoom | undo, redo ... import, export
    row = bar.withSizeKeepingCentre(bar.getWidth(), 30);
    place(pointerTool, 40);
    place(splitTool, 40);
    place(stretchTool, 40, 14);
    place(snap, 116, 14);
    place(zoomOut, 36);
    place(zoomIn, 36);
    place(zoomFit, 36, 14);
    place(undo, 38);
    place(redo, 38, 14);
    exportButton.setBounds(row.removeFromRight(96));
    row.removeFromRight(6);
    importButton.setBounds(row.removeFromRight(96));
    cancelExportButton.setBounds(exportButton.getBounds());
    exportBar.setBounds(importButton.getBounds().withLeft(importButton.getX() - 120).reduced(0, 3));

    horizontal.setBounds(area.removeFromBottom(12).withTrimmedLeft(headerWidth).withTrimmedRight(12));
    exportRange.setBounds(area.removeFromBottom(24).withTrimmedRight(12));
    chordBar.setBounds(area.removeFromTop(28).withTrimmedRight(12));
    vertical.setBounds(area.removeFromRight(12).withTrimmedTop(TimelineComponent::rulerHeight));
    headerColumn->setBounds(area.removeFromLeft(headerWidth));
    timeline.setBounds(area);
    layoutHeaders();
    updateScrollBars();
}

void ArrangementView::visibilityChanged()
{
    if (isShowing())
        timeline.grabKeyboardFocus();
}

bool ArrangementView::performCommand(juce::CommandID id)
{
    namespace cmd = commands;
    const auto selected = timeline.selectedTrack();
    const auto* track = selected >= 0 && selected < static_cast<int>(controller.state().tracks.size())
                      ? &controller.state().tracks[static_cast<size_t>(selected)] : nullptr;
    const auto needTrack = [this, track]
    {
        if (track == nullptr)
            status(text("Erst eine Spur auswählen (Klick auf den Spurkopf)."));
        return track != nullptr;
    };
    switch (id)
    {
        case cmd::playStop: controller.togglePlay(); return true;
        case cmd::record: controller.toggleRecord(selected); return true;
        case cmd::toStart: controller.locate(0); return true;
        case cmd::loop: loop.triggerClick(); return true;
        case cmd::metronome: metronome.triggerClick(); return true;
        case cmd::countIn: countIn.triggerClick(); return true;
        case cmd::undo: controller.undo(); return true;
        case cmd::redo: controller.redo(); return true;
        case cmd::split: timeline.splitAtPlayhead(); return true;
        case cmd::duplicate: timeline.duplicateSelection(); return true;
        case cmd::nextTake: timeline.stepTakes(1); return true;
        case cmd::previousTake: timeline.stepTakes(-1); return true;
        case cmd::copy: timeline.copySelection(); return true;
        case cmd::cut: timeline.cutSelection(); return true;
        case cmd::paste: timeline.paste(); return true;
        case cmd::selectAll: timeline.selectAll(); return true;
        case cmd::deleteSelection: timeline.deleteSelection(); return true;
        case cmd::cancel: controller.cancelGesture(); return true;
        case cmd::pointerTool: pointerTool.setToggleState(true, juce::sendNotificationSync); return true;
        case cmd::splitTool: splitTool.setToggleState(true, juce::sendNotificationSync); return true;
        case cmd::stretchTool: stretchTool.setToggleState(true, juce::sendNotificationSync); return true;
        case cmd::zoomIn: timeline.zoomBy(0.5, static_cast<float>(timeline.getWidth()) * 0.5f); return true;
        case cmd::zoomOut: timeline.zoomBy(2.0, static_cast<float>(timeline.getWidth()) * 0.5f); return true;
        case cmd::zoomFit: timeline.zoomToFit(); return true;
        case cmd::follow: followEnabled = ! followEnabled; status(followEnabled ? "Mitlaufen an" : "Mitlaufen aus"); return true;
        case cmd::newTrack: controller.addTrack({}, -1); return true;
        case cmd::newDrumTrack: openDrumMachine(controller.addDrumTrack(), 0); return true;
        case cmd::newReturnTrack: controller.addReturnTrack(); return true;
        case cmd::freezeTrack: if (needTrack()) controller.freezeTrack(track->id); return true;
        case cmd::unfreezeTrack: if (needTrack()) controller.unfreezeTrack(track->id); return true;
        case cmd::flattenTrack: if (needTrack()) controller.flattenTrack(track->id); return true;
        case cmd::trackFx: if (needTrack()) openTrackFx(track->id); return true;
        case cmd::trackSends:
            if (needTrack())
                for (auto& header : headers)
                    if (header->trackId() == track->id)
                        header->showSends();
            return true;
        case cmd::save: saveProject(false); return true;
        case cmd::saveAs: saveProject(true); return true;
        case cmd::open: openProject(); return true;
        case cmd::import: chooseImport(); return true;
        case cmd::exportAudio: showExportMenu(); return true;
        default: return false;
    }
}

//==============================================================================
void ArrangementView::chooseImport()
{
    chooser = std::make_shared<juce::FileChooser>("Audio oder Video importieren", lastExportDirectory, daw::codecs::importWildcard());
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
        [safe = juce::Component::SafePointer<ArrangementView>(this)](const juce::FileChooser& fileChooser)
        {
            if (safe == nullptr)
                return;
            juce::Array<juce::File> files;
            for (const auto& file : fileChooser.getResults())
                if (file.existsAsFile())
                    files.add(file);
            if (! files.isEmpty())
                safe->controller.importFiles(files, files.size() == 1 ? safe->timeline.selectedTrack() : -1, safe->controller.engine().position());
            safe->chooser.reset();
        });
}

void ArrangementView::showExportMenu()
{
    juce::PopupMenu formats;
    for (const auto& info : daw::codecs::exportFormats())
        formats.addItem(info.name, true, info.format == exportFormat, [this, format = info.format] { exportFormat = format; status(text("Exportformat: ") + daw::codecs::infoFor(format).name); });

    juce::PopupMenu menu;
    const auto& state = controller.state();
    const auto [from, to] = state.exportRange();
    menu.addSectionHeader(text("Format: ") + daw::codecs::infoFor(exportFormat).name);
    menu.addSectionHeader((state.hasExportRange() ? text("Bereich: ") : text("Bereich: ganzer Song, ")) + controller.grid().formatPosition(from)
                          + text(" – ") + controller.grid().formatPosition(to) + text(" (Balken unten)"));
    menu.addItem(text("Mix exportieren …"), [this] { exportTo(false); });
    menu.addItem(text("Alle Spuren einzeln (Stems) …"), [this] { exportTo(true); });
    if (const auto track = timeline.selectedTrack(); track >= 0 && track < static_cast<int>(controller.state().tracks.size()))
        menu.addItem(text("Ausgewählte Spur \"") + controller.state().tracks[static_cast<size_t>(track)].name + text("\" …"),
                     [this, id = controller.state().tracks[static_cast<size_t>(track)].id] { exportTo(false, id); });
    menu.addSeparator();
    menu.addSubMenu("Format", formats);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton));
}

void ArrangementView::exportTo(bool stems, const juce::Uuid& singleTrack)
{
    const auto& info = daw::codecs::infoFor(exportFormat);
    const auto directory = lastExportDirectory.isDirectory() ? lastExportDirectory : controller.projectFile().getParentDirectory();
    auto suggestion = controller.projectFile().getFileNameWithoutExtension();
    if (const auto index = controller.state().trackIndex(singleTrack); index >= 0)
        suggestion = controller.state().tracks[static_cast<size_t>(index)].name;
    const auto title = stems ? text("Ordner für die Stems wählen") : text("Export speichern als");
    chooser = std::make_shared<juce::FileChooser>(title, stems ? directory : directory.getChildFile(suggestion + "." + info.extension),
                                                  stems ? "*" : juce::String("*.") + info.extension);
    const auto browserFlags = stems ? juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories
                             : juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
    chooser->launchAsync(browserFlags, [safe = juce::Component::SafePointer<ArrangementView>(this), stems, singleTrack](const juce::FileChooser& fileChooser)
    {
        if (safe == nullptr)
            return;
        const auto target = fileChooser.getResult();
        safe->chooser.reset();
        if (target == juce::File())
            return;
        safe->lastExportDirectory = stems ? target : target.getParentDirectory();
        safe->status(text("Exportiere … ") + target.getFileName());
        safe->controller.exportAudio(target, safe->exportFormat, stems, [safe, target](juce::Result result)
        {
            if (safe != nullptr)
                safe->status(result.wasOk() ? text("Export fertig: ") + target.getFullPathName() : result.getErrorMessage());
        }, singleTrack);
    });
}

void ArrangementView::showProjectMenu()
{
    juce::PopupMenu menu;
    menu.addItem("Neues Projekt", [this]
    {
        autosave();
        controller.newProject();
        status(text("Neues Projekt – wird beim ersten Speichern bzw. bei der ersten Aufnahme angelegt."));
    });
    menu.addItem(text("Projekt öffnen …\tCtrl+O"), [this] { openProject(); });
    menu.addItem("Speichern\tCtrl+S", [this] { saveProject(false); });
    menu.addItem(text("Speichern unter …\tCtrl+Shift+S"), [this] { saveProject(true); });
    menu.addSeparator();
    menu.addItem(text("Aufnahme-Latenz feinjustieren …"), [this] { editLatencyOffset(); });
    const auto ffmpeg = daw::codecs::findFfmpeg();
    menu.addItem(ffmpeg.existsAsFile() ? text("FFmpeg: ") + ffmpeg.getFullPathName() : text("FFmpeg auswählen … (für AAC, Opus, Video usw.)"), [this] { chooseFfmpeg(); });
    menu.addItem(text("Projektordner öffnen"), [this] { controller.projectFile().getParentDirectory().startAsProcess(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&projectButton));
}

void ArrangementView::showAddTrackMenu(juce::Component& target)
{
    juce::PopupMenu menu;
    menu.addSectionHeader(text("Neue Spur"));
    menu.addItem(text("Audiospur mit NAM-Rig (bearbeitet)\tCtrl+T"), [this] { controller.addTrack({}, -1); });
    for (const auto& input : controller.availableInputs())
        if (input.channel >= 0)
            menu.addItem(text("Audiospur mit ") + input.name + text(" (trocken)"), [this, channel = input.channel] { controller.addTrack({}, channel); });
    menu.addItem(text("Drum-Spur (Drumcomputer)"), [this] { openDrumMachine(controller.addDrumTrack(), 0); });
    menu.addItem(text("MIDI-Spur (Synth, Vital …)"), [this] { controller.addMidiTrack(); });
    menu.addSeparator();
    menu.addItem(text("Return-Spur (Send-Effekt: Hall, Delay …)"), daw::edit::makeReturnTrack(controller.state()).has_value(), false,
                 [this] { controller.addReturnTrack(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target));
}

void ArrangementView::openProject()
{
    chooser = std::make_shared<juce::FileChooser>(text("Projekt öffnen"), controller.projectFile().getParentDirectory().getParentDirectory(), "*.tonproj");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<ArrangementView>(this)](const juce::FileChooser& fileChooser)
        {
            if (safe == nullptr)
                return;
            const auto file = fileChooser.getResult();
            safe->chooser.reset();
            if (! file.existsAsFile())
                return;
            safe->autosave();
            const auto result = safe->controller.openProject(file);
            safe->status(result.wasOk() ? text("Projekt geöffnet: ") + file.getFileNameWithoutExtension() : result.getErrorMessage());
            safe->timeline.zoomToFit();
        });
}

void ArrangementView::saveProject(bool forceChooser)
{
    if (! forceChooser)
    {
        const auto result = controller.save();
        status(result.wasOk() ? "Gespeichert: " + controller.projectFile().getFullPathName() : result.getErrorMessage());
        stateChanged();
        return;
    }
    chooser = std::make_shared<juce::FileChooser>("Projekt speichern unter", controller.projectFile(), "*.tonproj");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<ArrangementView>(this)](const juce::FileChooser& fileChooser)
        {
            if (safe == nullptr)
                return;
            auto file = fileChooser.getResult();
            safe->chooser.reset();
            if (file == juce::File())
                return;
            // Each project gets its own folder with an Audio subfolder.
            if (file.getParentDirectory().getFileName() != file.getFileNameWithoutExtension())
                file = file.getParentDirectory().getChildFile(file.getFileNameWithoutExtension()).getChildFile(file.getFileName());
            const auto result = safe->controller.saveAs(file);
            safe->status(result.wasOk() ? "Gespeichert: " + safe->controller.projectFile().getFullPathName() : result.getErrorMessage());
            safe->stateChanged();
        });
}

void ArrangementView::chooseFfmpeg()
{
   #if JUCE_WINDOWS
    const juce::String pattern = "ffmpeg.exe";
   #else
    const juce::String pattern = "ffmpeg*";
   #endif
    chooser = std::make_shared<juce::FileChooser>(text("FFmpeg-Programm auswählen"), juce::File(), pattern);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<ArrangementView>(this)](const juce::FileChooser& fileChooser)
        {
            if (safe == nullptr)
                return;
            const auto file = fileChooser.getResult();
            safe->chooser.reset();
            if (! file.existsAsFile())
                return;
            daw::codecs::setFfmpegOverride(file);
            safe->status("FFmpeg: " + file.getFullPathName());
        });
}

void ArrangementView::editLatencyOffset()
{
    auto* window = new juce::AlertWindow(text("Aufnahme-Latenz"),
        text("Die Treiberlatenz wird automatisch ausgeglichen. Liegen Aufnahmen trotzdem zu spät (+) oder zu früh (–), hier in Millisekunden korrigieren."),
        juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("ms", juce::String(controller.manualLatencyMs(), 2), "Zusatz-Offset (ms)");
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Abbrechen", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<ArrangementView>(this), window](int result)
    {
        if (safe == nullptr || result != 1)
            return;
        safe->controller.setManualLatencyMs(juce::jlimit(-500.0, 500.0, window->getTextEditorContents("ms").getDoubleValue()));
        safe->controller.markDirty();
    }), true);
}
} // namespace nam::ui
