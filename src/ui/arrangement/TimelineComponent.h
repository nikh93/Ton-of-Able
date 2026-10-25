#pragma once

#include <optional>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"

namespace nam::ui
{
/** The arrangement canvas: ruler with loop brace, track lanes, clips with
    waveforms, playhead and all mouse editing (move, copy, trim, stretch,
    fades, split, marquee selection, file drops, zoom and scroll). */
class TimelineComponent final : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    enum class Tool { pointer, split, stretch };
    static constexpr int rulerHeight = 34;

    explicit TimelineComponent(daw::ArrangementController&);

    // View.
    void setTool(Tool newTool);
    [[nodiscard]] Tool tool() const noexcept { return currentTool; }
    void setSnapDivision(double bars) noexcept { snapBars = bars; }
    [[nodiscard]] double snapDivision() const noexcept { return snapBars; }
    void zoomBy(double factor, float anchorX);
    void zoomToFit();
    void setTrackHeight(int height);
    [[nodiscard]] int trackHeight() const noexcept { return laneHeight; }
    void setScrollY(int value);
    [[nodiscard]] int scrollY() const noexcept { return verticalOffset; }
    void setViewStart(double sample);
    [[nodiscard]] double viewStart() const noexcept { return firstSample; }
    [[nodiscard]] double samplesPerPixel() const noexcept { return zoom; }
    [[nodiscard]] int contentHeight() const;
    [[nodiscard]] double contentLengthSamples() const;
    void followPlayhead();
    // Mapping between timeline x and samples (shared by the export range bar).
    [[nodiscard]] double xToSampleExact(float x) const noexcept { return firstSample + static_cast<double>(x) * zoom; }
    [[nodiscard]] juce::int64 xToSample(float x) const noexcept;
    [[nodiscard]] float sampleToX(double sample) const noexcept { return static_cast<float>((sample - firstSample) / zoom); }
    /** Snaps to the grid set in the toolbar; Alt disables it. */
    [[nodiscard]] juce::int64 snapPosition(juce::int64 position, const juce::ModifierKeys& mods) const { return snap(position, mods); }
    /** Samples shown from the left to the right edge. */
    [[nodiscard]] std::pair<juce::int64, juce::int64> visibleRange() const noexcept;
    /** Start and end of the selected clips, or nothing without a selection. */
    [[nodiscard]] std::optional<std::pair<juce::int64, juce::int64>> selectionRange() const;

    // Selection and commands (also reached through keyboard shortcuts).
    [[nodiscard]] int selectedTrack() const noexcept { return currentTrack; }
    void selectTrack(int index);
    void selectAll();
    void splitAtPlayhead();
    void deleteSelection();
    /** Loop recordings: the next (+1) or previous (-1) take of the selected clips. */
    void stepTakes(int direction);
    void duplicateSelection();
    void copySelection();
    void cutSelection();
    void paste();
    void stretchSelection(double ratio);
    void transposeSelection(double semitones, bool relative);
    void stateChanged();

    std::function<void()> onViewChanged;
    std::function<void()> onSelectionChanged;
    std::function<void(const juce::Uuid& trackId, int pattern)> onOpenDrums;
    /** Drum tracks: the drum arrangement editor (sections, groove library, auto mode) at this bar. */
    std::function<void(const juce::Uuid& trackId, int bar)> onOpenDrumArrangement;
    std::function<void(const juce::Uuid& clipId)> onOpenPianoRoll;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    /** In the loop strip: -1 near the loop start, 1 near its end, 0 elsewhere. */
    [[nodiscard]] int loopEdgeAt(float x) const;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void fileDragMove(const juce::StringArray&, int x, int y) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int x, int y) override;

private:
    enum class Zone { none, body, leftEdge, rightEdge, fadeIn, fadeOut };
    enum class Drag { none, move, trimStart, trimEnd, stretchStart, stretchEnd, fadeIn, fadeOut, marquee, scrub, loopCreate, loopMove, loopStart, loopEnd };

    struct Hit
    {
        const daw::Clip* clip = nullptr;
        int track = -1;
        Zone zone = Zone::none;
    };

    struct Origin
    {
        juce::Uuid id;
        juce::Uuid copyId;
        juce::int64 start = 0;
        int track = 0;
    };

    [[nodiscard]] int laneTop(int track) const noexcept { return rulerHeight + track * laneHeight - verticalOffset; }
    [[nodiscard]] int trackAt(int y) const noexcept;
    [[nodiscard]] juce::Rectangle<float> clipBounds(const daw::Clip&, int track) const noexcept;
    [[nodiscard]] Hit hitTest(juce::Point<float>) const;
    [[nodiscard]] juce::int64 snap(juce::int64 position, const juce::ModifierKeys&) const;
    [[nodiscard]] bool isSelected(const juce::Uuid& id) const { return selection.contains(id); }
    [[nodiscard]] double gridStepBars() const;

    void paintRuler(juce::Graphics&);
    void paintGrid(juce::Graphics&, juce::Rectangle<int> lanes);
    void paintClip(juce::Graphics&, const daw::Clip&, int track, const daw::Track&);
    void paintPattern(juce::Graphics&, const daw::Clip&, const daw::Track&, juce::Rectangle<float> area);
    void paintMidi(juce::Graphics&, const daw::Clip&, const daw::Track&, juce::Rectangle<float> area);
    void showTakeMenu(const juce::Uuid& clipId);
    [[nodiscard]] juce::Rectangle<float> takeBadge(juce::Rectangle<float> clipBounds) const;
    void showClipMenu(const daw::Clip&, juce::int64 at);
    void showLaneMenu(int track, juce::int64 at);
    void showClipProperties(const juce::Uuid& id);
    void notifyView();

    daw::ArrangementController& controller;
    Tool currentTool = Tool::pointer;
    double snapBars = 0.25;
    double zoom = 400.0;           // samples per pixel
    double firstSample = 0.0;
    int verticalOffset = 0;
    int laneHeight = 92;
    int currentTrack = 0;
    juce::Array<juce::Uuid> selection;

    Drag drag = Drag::none;
    bool dragMoved = false;
    bool dragCopies = false;
    juce::Uuid dragClip;
    juce::int64 dragAnchorSample = 0;
    int dragAnchorTrack = 0;
    juce::int64 dragClipStart = 0;
    juce::int64 loopDragStart = 0, loopDragEnd = 0;
    std::vector<Origin> origins;
    juce::Rectangle<float> marquee;
    juce::Point<float> mouseDownPosition;
    int dropTrack = -2;
    float dropX = 0.0f;
};
} // namespace nam::ui
