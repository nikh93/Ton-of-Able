#pragma once

#include <map>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "TimelineComponent.h"
#include "AccompanimentPanel.h"
#include "ChordProgressionEditor.h"
#include "ChordTrackBar.h"
#include "ExportRangeBar.h"
#include "PianoRollComponent.h"
#include "ui/components/Icons.h"
#include "TrackHeaderComponent.h"

namespace nam::ui
{
/** Multitrack arrangement view: transport and tool bars, track headers,
    timeline and scroll bars. Keyboard commands arrive through the app's
    command manager; their default keys follow Ableton Live where possible
    (Space, F9, Ctrl+E, Ctrl+D, Ctrl+T, Ctrl+L, Ctrl+Z ...) and can be changed. */
class ArrangementView final : public juce::Component, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    explicit ArrangementView(daw::ArrangementController&);
    ~ArrangementView() override;

    /** UI preferences (snap, zoom, export format, FFmpeg path, last project). */
    [[nodiscard]] juce::ValueTree saveState() const;
    void restoreState(const juce::ValueTree&);
    /** Called on shutdown: saves the project if it has unsaved changes. */
    void autosave();

    std::function<void(const juce::String&)> onStatus;
    /** Opens the shortcut overview (owned by the app). */
    std::function<void()> onShowShortcuts;
    /** Supplies the editor for a track's effect chain (needs the app's libraries). */
    std::function<std::unique_ptr<juce::Component>(const juce::Uuid& trackId)> createTrackFxEditor;
    void openTrackFx(const juce::Uuid& trackId);
    /** Supplies the drum machine editor (needs the app's plugin hosting). */
    std::function<std::unique_ptr<juce::Component>(const juce::Uuid& trackId, int pattern)> createDrumEditor;
    void openDrumMachine(const juce::Uuid& trackId, int pattern);
    /** Instrument menu of a MIDI track (built-in synth, VST3 instruments), set by the main window. */
    std::function<void(const juce::Uuid& trackId, juce::Component& target)> showInstrumentMenu;
    /** Effect settings for the "Atmo-Hall" return of accompaniments (e.g. Valhalla Supermassive). */
    std::function<juce::ValueTree()> createSpaceEffect;
    /** "Begleitung aus Akkorden": creates tracks (null id) or edits one accompaniment track. */
    void openAccompaniment(const juce::Uuid& trackId);
    /** The chord progression editor (sections, library, auto mode), starting at bar. */
    void openProgressionEditor(int bar);
    void openPianoRoll(const juce::Uuid& clipId);

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    /** Executes a keyboard command (see ui/shortcuts/Shortcuts.h); false if unknown. */
    bool performCommand(juce::CommandID);

private:
    class HeaderColumn;
    static constexpr int headerWidth = 250;
    static constexpr int toolbarHeight = 88;

    void timerCallback() override;
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    void stateChanged();
    void rebuildHeaders();
    void layoutHeaders();
    void updateScrollBars();
    void updateTransportButtons();
    void status(const juce::String& text) { if (onStatus) onStatus(text); }

    void chooseImport();
    void showExportMenu();
    void exportTo(bool stems, const juce::Uuid& singleTrack = juce::Uuid::null());
    void showProjectMenu();
    void showAddTrackMenu(juce::Component& target);
    void openProject();
    void saveProject(bool forceChooser);
    void chooseFfmpeg();
    void editLatencyOffset();

    daw::ArrangementController& controller;
    TimelineComponent timeline { controller };
    ExportRangeBar exportRange { controller, timeline, headerWidth };
    ChordTrackBar chordBar { controller, timeline, headerWidth };
    std::unique_ptr<HeaderColumn> headerColumn;
    std::vector<std::unique_ptr<TrackHeaderComponent>> headers;
    juce::ScrollBar horizontal { false }, vertical { true };

    IconButton toStart { Icon::toStart }, play { Icon::play }, stopButton { Icon::stop }, record { Icon::record };
    IconButton loop { Icon::loop }, metronome { Icon::metronome }, countIn { Icon::countIn };
    juce::Label position, clock, bpmLabel { {}, "BPM" }, bpm, masterLabel { {}, "MASTER" };
    juce::ComboBox signature, snap;
    juce::Slider master { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    IconButton pointerTool { Icon::pointer }, splitTool { Icon::scissors }, stretchTool { Icon::stretch };
    IconButton zoomOut { Icon::zoomOut }, zoomIn { Icon::zoomIn }, zoomFit { Icon::zoomFit };
    IconButton undo { Icon::undo }, redo { Icon::redo };
    IconButton importButton { Icon::importFile, "Import" }, exportButton { Icon::exportFile, "Export" };
    IconButton projectButton { Icon::folder }, shortcutsButton { Icon::keyboard };
    // Shown instead of Import/Export while an export runs.
    double exportFraction = 0.0;
    juce::ProgressBar exportBar { exportFraction };
    juce::TextButton cancelExportButton;
    juce::Rectangle<int> displayArea; // the dark "LCD" behind position, tempo and time signature

    daw::codecs::ExportFormat exportFormat = daw::codecs::ExportFormat::wav24;
    juce::File lastExportDirectory;
    std::shared_ptr<juce::FileChooser> chooser;
    bool followEnabled = true;
    std::map<juce::Uuid, std::unique_ptr<juce::DocumentWindow>> fxWindows, drumWindows, pianoWindows;
    std::unique_ptr<juce::DocumentWindow> accompanimentWindow, progressionWindow;
};
} // namespace nam::ui
