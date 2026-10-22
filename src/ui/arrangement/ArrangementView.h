#pragma once

#include <map>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "TimelineComponent.h"
#include "TrackHeaderComponent.h"

namespace nam::ui
{
/** Multitrack arrangement view: transport and tool bars, track headers,
    timeline and scroll bars. Keyboard shortcuts follow Ableton Live where
    possible (Space, F9, Ctrl+E, Ctrl+D, Ctrl+T, Ctrl+L, Ctrl+Z ...). */
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
    /** Supplies the editor for a track's effect chain (needs the app's libraries). */
    std::function<std::unique_ptr<juce::Component>(const juce::Uuid& trackId)> createTrackFxEditor;
    void openTrackFx(const juce::Uuid& trackId);
    /** Supplies the drum machine editor (needs the app's plugin hosting). */
    std::function<std::unique_ptr<juce::Component>(const juce::Uuid& trackId, int pattern)> createDrumEditor;
    void openDrumMachine(const juce::Uuid& trackId, int pattern);

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    class HeaderColumn;

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
    void exportTo(bool stems, bool loopOnly, const juce::Uuid& singleTrack = juce::Uuid::null());
    void showProjectMenu();
    void openProject();
    void saveProject(bool forceChooser);
    void chooseFfmpeg();
    void editLatencyOffset();

    daw::ArrangementController& controller;
    TimelineComponent timeline { controller };
    std::unique_ptr<HeaderColumn> headerColumn;
    std::vector<std::unique_ptr<TrackHeaderComponent>> headers;
    juce::ScrollBar horizontal { false }, vertical { true };

    juce::TextButton toStart, play, stopButton { "Stop" }, record { "REC" }, loop { "Loop" }, metronome { "Click" }, countIn { "Count-In" };
    juce::Label position, bpmLabel { {}, "BPM" }, bpm, masterLabel { {}, "Master" };
    juce::ComboBox signature, snap;
    juce::Slider master { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton pointerTool { "Zeiger" }, splitTool { "Schere" }, stretchTool { "Stretch" };
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, zoomFit { "Alles" };
    juce::TextButton undo, redo, addTrack { "+ Spur" }, importButton { "Import" }, exportButton { "Export" }, projectButton { "Projekt" };
    juce::Label projectName;

    daw::codecs::ExportFormat exportFormat = daw::codecs::ExportFormat::wav24;
    juce::File lastExportDirectory;
    std::shared_ptr<juce::FileChooser> chooser;
    bool followEnabled = true;
    std::map<juce::Uuid, std::unique_ptr<juce::DocumentWindow>> fxWindows, drumWindows;
};
} // namespace nam::ui
