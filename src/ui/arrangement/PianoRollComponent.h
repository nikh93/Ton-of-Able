#pragma once

#include <set>

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"

namespace nam::ui
{
/** Editor for the notes of a MIDI clip: keyboard on the left, note grid,
    velocity lane below. Click on empty space draws a note (drag sets its
    length), drag notes to move them, their right edge to change the length;
    Shift+drag selects with a rectangle, double-click or right-click deletes.
    Every edit is one undo step of the arrangement. */
class PianoRollComponent final : public juce::Component, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    struct Host
    {
        /** Instrument menu of the clip's track (built-in synth, VST3 instruments). */
        std::function<void(const juce::Uuid& trackId, juce::Component& target)> showInstrumentMenu;
    };

    PianoRollComponent(daw::ArrangementController&, const juce::Uuid& clipId, Host);
    ~PianoRollComponent() override;

    [[nodiscard]] const juce::Uuid& clip() const noexcept { return clipId; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    enum class Drag { none, draw, move, resize, marquee, velocity, keyboard };

    void timerCallback() override;
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    [[nodiscard]] const daw::Clip* clipData() const;
    [[nodiscard]] const daw::Track* trackData() const;
    [[nodiscard]] daw::MidiNotes notes() const;
    [[nodiscard]] double samplesPerBeat() const;
    /** Content beats at the clip start and its length in beats. */
    [[nodiscard]] double firstClipBeat() const;
    [[nodiscard]] double clipBeats() const;
    [[nodiscard]] double gridBeats() const;
    [[nodiscard]] double snapBeat(double beat, const juce::ModifierKeys&) const;

    // Geometry.
    [[nodiscard]] juce::Rectangle<int> gridArea() const;
    [[nodiscard]] juce::Rectangle<int> keyboardArea() const;
    [[nodiscard]] juce::Rectangle<int> velocityArea() const;
    [[nodiscard]] float beatToX(double beat) const;
    [[nodiscard]] double xToBeat(float x) const;
    [[nodiscard]] float pitchToY(int pitch) const;
    [[nodiscard]] int yToPitch(float y) const;
    [[nodiscard]] juce::Rectangle<float> noteBounds(const daw::MidiNote&) const;
    /** Index of the note under the point, -1 if none; edge is true near its end. */
    [[nodiscard]] int noteAt(juce::Point<float>, bool& edge) const;

    void commit(daw::MidiNotes, const juce::String& description);
    /** During a mouse gesture: shows list (dragOrigin edited) with dragSelection selected. */
    void updateDrag(daw::MidiNotes list);
    void deleteSelected();
    void quantizeSelected();
    void transposeSelected(int semitones);
    void moveSelected(double beats);
    void duplicateSelected();
    void selectAll();
    void preview(int pitch, int velocity);
    void updateScrollBars();
    void showMenu(juce::Point<float> at);

    daw::ArrangementController& controller;
    juce::Uuid clipId, trackId;
    Host host;
    std::set<int> selection;                 // note indices into the clip's notes
    double pixelsPerBeat = 80.0, firstBeat = 0.0;
    float rowHeight = 14.0f, topPitch = 84.0f; // pitch at the top edge (fractional while scrolling)
    double lastLength = 1.0;                 // length of the next drawn note, beats

    Drag drag = Drag::none;
    juce::Point<float> downPosition;
    daw::MidiNotes dragOrigin;               // the notes at mouse-down (plus a drawn one)
    std::set<int> dragSelection;             // selected indices into dragOrigin
    int dragNote = -1, previewPitch = -1;
    juce::Rectangle<float> marquee;

    juce::Label title, hint;
    juce::ComboBox gridChoice;
    juce::TextButton instrumentButton, quantizeButton, zoomInButton { "+" }, zoomOutButton { "-" };
    juce::ScrollBar horizontal { false }, vertical { true };
};
} // namespace nam::ui
