#include "ArrangementView.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

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
        addTrackButton.setTooltip(text("Neue Audiospur (Ctrl+T)"));
        addTrackButton.onClick = [this] { view.controller.addTrack({}, -1); };
        addAndMakeVisible(addTrackButton);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff201a15));
        auto top = getLocalBounds().removeFromTop(TimelineComponent::rulerHeight);
        g.setColour(juce::Colour(0xff2b231c));
        g.fillRect(top);
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("SPUREN", top.reduced(10, 0), juce::Justification::centredLeft);
    }

    void resized() override
    {
        addTrackButton.setBounds(getLocalBounds().removeFromTop(TimelineComponent::rulerHeight).removeFromRight(34).reduced(4, 6));
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
    juce::TextButton addTrackButton { "+" };
};

ArrangementView::ArrangementView(daw::ArrangementController& c) : controller(c)
{
    headerColumn = std::make_unique<HeaderColumn>(*this);
    addAndMakeVisible(*headerColumn);
    addAndMakeVisible(timeline);
    for (auto* bar : { &horizontal, &vertical })
    {
        bar->setAutoHide(false);
        bar->setColour(juce::ScrollBar::thumbColourId, Theme::amber().withAlpha(0.45f));
        bar->setColour(juce::ScrollBar::trackColourId, juce::Colour(0xff15110d));
        bar->addListener(this);
        addAndMakeVisible(*bar);
    }

    toStart.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x8f\xae")));
    play.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6 Play")));
    record.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f REC")));
    undo.setButtonText(text("↶ Undo"));
    redo.setButtonText(text("↷ Redo"));
    toStart.setTooltip("Zum Anfang (Pos1)");
    play.setTooltip("Abspielen / Stoppen (Leertaste)");
    stopButton.setTooltip(text("Stopp – zweimal: zurück zum Anfang"));
    record.setTooltip(text("Aufnahme auf allen scharfen Spuren (F9). Ohne scharfe Spur wird die ausgewählte Spur verwendet."));
    loop.setTooltip(text("Loop an/aus (Ctrl+L). Loop-Bereich oben im Lineal ziehen."));
    metronome.setTooltip("Metronom");
    countIn.setTooltip(text("Ein Takt Vorzähler vor der Aufnahme"));
    undo.setTooltip(text("Rückgängig (Ctrl+Z)"));
    redo.setTooltip("Wiederholen (Ctrl+Y / Ctrl+Shift+Z)");
    pointerTool.setTooltip(text("Zeiger (V): verschieben, Kanten ziehen = kürzen, Ctrl+Kante = Time-Stretch, Ctrl+Ziehen = kopieren, Ecken oben = Fades, Alt = ohne Raster"));
    splitTool.setTooltip(text("Schere (C): Klick teilt den Clip"));
    stretchTool.setTooltip(text("Stretch (T): Kanten ziehen = Time-Stretch mit Rubber Band"));
    zoomIn.setTooltip("Hineinzoomen (Ctrl+Mausrad / Ctrl++)");
    zoomOut.setTooltip("Herauszoomen (Ctrl+Mausrad / Ctrl+-)");
    zoomFit.setTooltip(text("Alles zeigen"));
    importButton.setTooltip(text("Audio oder Video importieren (Ctrl+I) – auch per Drag & Drop auf die Timeline"));
    exportButton.setTooltip("Mix oder einzelne Spuren exportieren");

    for (auto* button : { &toStart, &play, &stopButton, &record, &loop, &metronome, &countIn, &pointerTool, &splitTool, &stretchTool,
                          &zoomOut, &zoomIn, &zoomFit, &undo, &redo, &addTrack, &importButton, &exportButton, &projectButton })
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

    toStart.onClick = [this] { controller.locate(0); };
    play.onClick = [this] { controller.togglePlay(); };
    stopButton.onClick = [this] { controller.stop(); };
    record.onClick = [this] { controller.toggleRecord(timeline.selectedTrack()); };
    loop.onClick = [this]
    {
        const auto& state = controller.state();
        auto start = state.loopStart, end = state.loopEnd;
        if (end <= start)
        {
            // No loop yet: four bars from the playhead.
            start = controller.grid().snap(controller.engine().position(), 1.0);
            end = start + static_cast<juce::int64>(controller.grid().samplesPerBar() * 4.0);
        }
        controller.setLoop(! state.loopEnabled, start, end);
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
    addTrack.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem("Spur mit NAM-Rig (bearbeitet)", [this] { controller.addTrack({}, -1); });
        for (const auto& input : controller.availableInputs())
            if (input.channel >= 0)
                menu.addItem("Spur mit " + input.name, [this, channel = input.channel] { controller.addTrack({}, channel); });
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addTrack));
    };
    importButton.onClick = [this] { chooseImport(); };
    exportButton.onClick = [this] { showExportMenu(); };
    projectButton.onClick = [this] { showProjectMenu(); };

    position.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::bold));
    position.setColour(juce::Label::textColourId, Theme::amberBright());
    position.setColour(juce::Label::backgroundColourId, juce::Colour(0xff15110d));
    position.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(position);

    bpm.setEditable(false, true, false);
    bpm.setColour(juce::Label::backgroundColourId, juce::Colour(0xff15110d));
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
        label->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    addAndMakeVisible(bpm);

    for (size_t index = 0; index < std::size(signatures); ++index)
        signature.addItem(juce::String(signatures[index].beats) + "/" + juce::String(signatures[index].unit), static_cast<int>(index) + 1);
    signature.setTooltip("Taktart");
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

    projectName.setColour(juce::Label::textColourId, Theme::subduedText());
    projectName.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(projectName);

    timeline.onViewChanged = [this] { layoutHeaders(); updateScrollBars(); };
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
        setResizeLimits(860, 700, 1600, 1300);
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

ArrangementView::~ArrangementView()
{
    stopTimer();
    fxWindows.clear();
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
    projectName.setText(controller.projectFile().getFileNameWithoutExtension() + (controller.hasUnsavedChanges() ? " *" : ""), juce::dontSendNotification);
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
                header->onExportTrack = [this](const juce::Uuid& id) { exportTo(false, false, id); };
                header->onOpenFx = [this](const juce::Uuid& id) { openTrackFx(id); };
                headerColumn->addAndMakeVisible(*header);
                next.push_back(std::move(header));
            }
        }
        headers = std::move(next); // headers of removed tracks are destroyed here
        for (auto it = fxWindows.begin(); it != fxWindows.end();)
            it = controller.state().findTrack(it->first) == nullptr ? fxWindows.erase(it) : std::next(it);
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
    record.setColour(juce::TextButton::textColourOffId, Theme::danger());
    loop.setToggleState(controller.state().loopEnabled, juce::dontSendNotification);
    metronome.setToggleState(engine.isMetronomeOn(), juce::dontSendNotification);
    countIn.setToggleState(controller.isCountInEnabled(), juce::dontSendNotification);
}

void ArrangementView::timerCallback()
{
    auto& engine = controller.engine();
    const auto grid = controller.grid();
    position.setText(grid.formatPosition(engine.position()) + "   " + formatTime(engine.position(), grid.sampleRate), juce::dontSendNotification);
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
    if (engine.isRecording())
        record.setAlpha((juce::Time::getMillisecondCounter() / 400) % 2 == 0 ? 1.0f : 0.55f);
    else
        record.setAlpha(1.0f);
}

//==============================================================================
void ArrangementView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff17130f));
    auto bars = getLocalBounds().removeFromTop(84);
    g.setColour(juce::Colour(0xff2a221b));
    g.fillRoundedRectangle(bars.toFloat().reduced(0.0f, 2.0f), 8.0f);
}

void ArrangementView::resized()
{
    auto area = getLocalBounds();
    auto bars = area.removeFromTop(84).reduced(8, 6);
    auto row = bars.removeFromTop(32);
    const auto place = [&row](juce::Component& component, int width, int gap = 4)
    {
        component.setBounds(row.removeFromLeft(width));
        row.removeFromLeft(gap);
    };
    place(toStart, 36);
    place(play, 72);
    place(stopButton, 52);
    place(record, 66, 10);
    place(loop, 54);
    place(metronome, 52);
    place(countIn, 74, 12);
    place(position, 210, 12);
    place(bpmLabel, 30, 2);
    place(bpm, 58, 6);
    place(signature, 62, 12);
    projectButton.setBounds(row.removeFromRight(80));
    row.removeFromRight(10);
    master.setBounds(row.removeFromRight(juce::jmin(140, std::max(60, row.getWidth() - 46))));
    masterLabel.setBounds(row.removeFromRight(46));

    bars.removeFromTop(6);
    row = bars.removeFromTop(28);
    place(pointerTool, 66, 2);
    place(splitTool, 66, 2);
    place(stretchTool, 66, 12);
    place(snap, 108, 12);
    place(zoomOut, 28, 2);
    place(zoomIn, 28, 2);
    place(zoomFit, 52, 12);
    place(undo, 74, 2);
    place(redo, 74, 12);
    place(addTrack, 66, 4);
    place(importButton, 66, 4);
    place(exportButton, 66, 8);
    projectName.setBounds(row);

    area.removeFromTop(6);
    horizontal.setBounds(area.removeFromBottom(14).withTrimmedLeft(220).withTrimmedRight(14));
    vertical.setBounds(area.removeFromRight(14).withTrimmedTop(TimelineComponent::rulerHeight));
    headerColumn->setBounds(area.removeFromLeft(220));
    timeline.setBounds(area);
    layoutHeaders();
    updateScrollBars();
}

void ArrangementView::visibilityChanged()
{
    if (isShowing())
        timeline.grabKeyboardFocus();
}

bool ArrangementView::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto command = mods.isCommandDown();
    const auto code = key.getKeyCode();
    const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (code == juce::KeyPress::spaceKey) { controller.togglePlay(); return true; }
    if (code == juce::KeyPress::F9Key) { controller.toggleRecord(timeline.selectedTrack()); return true; }
    if (code == juce::KeyPress::homeKey) { controller.locate(0); return true; }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { timeline.deleteSelection(); return true; }
    if (code == juce::KeyPress::escapeKey) { controller.cancelGesture(); return true; }
    if (command)
    {
        const auto upper = juce::CharacterFunctions::toUpperCase(static_cast<juce::juce_wchar>(code));
        switch (upper)
        {
            case 'Z': mods.isShiftDown() ? controller.redo() : controller.undo(); return true;
            case 'Y': controller.redo(); return true;
            case 'E': timeline.splitAtPlayhead(); return true;
            case 'D': timeline.duplicateSelection(); return true;
            case 'C': timeline.copySelection(); return true;
            case 'X': timeline.cutSelection(); return true;
            case 'V': timeline.paste(); return true;
            case 'A': timeline.selectAll(); return true;
            case 'T': controller.addTrack({}, -1); return true;
            case 'I': chooseImport(); return true;
            case 'L': loop.triggerClick(); return true;
            case 'S': saveProject(mods.isShiftDown()); return true;
            case 'O': openProject(); return true;
            case '+':
            case '=': timeline.zoomBy(0.5, static_cast<float>(timeline.getWidth()) * 0.5f); return true;
            case '-': timeline.zoomBy(2.0, static_cast<float>(timeline.getWidth()) * 0.5f); return true;
            default: break;
        }
        return false;
    }
    switch (character)
    {
        case 'v': pointerTool.setToggleState(true, juce::sendNotificationSync); return true;
        case 'c': splitTool.setToggleState(true, juce::sendNotificationSync); return true;
        case 't': stretchTool.setToggleState(true, juce::sendNotificationSync); return true;
        case 'f': followEnabled = ! followEnabled; status(followEnabled ? "Mitlaufen an" : "Mitlaufen aus"); return true;
        case 'z': timeline.zoomToFit(); return true;
        default: break;
    }
    return false; // e.g. Tab bubbles up to switch views
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
    menu.addSectionHeader(text("Format: ") + daw::codecs::infoFor(exportFormat).name);
    menu.addItem(text("Mix exportieren …"), [this] { exportTo(false, false); });
    menu.addItem(text("Mix – nur Loop-Bereich …"), controller.state().loopEnabled, false, [this] { exportTo(false, true); });
    menu.addItem(text("Alle Spuren einzeln (Stems) …"), [this] { exportTo(true, false); });
    if (const auto track = timeline.selectedTrack(); track >= 0 && track < static_cast<int>(controller.state().tracks.size()))
        menu.addItem(text("Ausgewählte Spur \"") + controller.state().tracks[static_cast<size_t>(track)].name + text("\" …"),
                     [this, id = controller.state().tracks[static_cast<size_t>(track)].id] { exportTo(false, false, id); });
    menu.addSeparator();
    menu.addSubMenu("Format", formats);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton));
}

void ArrangementView::exportTo(bool stems, bool loopOnly, const juce::Uuid& singleTrack)
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
    chooser->launchAsync(browserFlags, [safe = juce::Component::SafePointer<ArrangementView>(this), stems, loopOnly, singleTrack](const juce::FileChooser& fileChooser)
    {
        if (safe == nullptr)
            return;
        const auto target = fileChooser.getResult();
        safe->chooser.reset();
        if (target == juce::File())
            return;
        safe->lastExportDirectory = stems ? target : target.getParentDirectory();
        safe->status(text("Exportiere … ") + target.getFileName());
        safe->controller.exportAudio(target, safe->exportFormat, loopOnly, stems, [safe, target](juce::Result result)
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
