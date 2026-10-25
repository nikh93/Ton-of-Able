#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"
#include "TimelineComponent.h"

namespace nam::ui
{
/** The lead sheet above the tracks: one chord per bar or per half bar.
    Click a bar and type ("Am7", "G/H"; two chords such as "Am G" split the
    bar), Enter keeps it, Tab goes on to the next bar, Esc cancels;
    right-click switches between whole and half bars or clears. */
class ChordTrackBar final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChordTrackBar(daw::ArrangementController&, const TimelineComponent&, int labelWidth);
    ~ChordTrackBar() override;
    /** Repaints when the view or the chords changed (called from the view's timer). */
    void update();
    /** "Begleitung erzeugen …" in the context menu. */
    std::function<void()> onAccompaniment;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    class Editor;
    [[nodiscard]] float barToX(double bar) const;
    [[nodiscard]] double xToBar(float x) const;
    [[nodiscard]] double samplesPerBar() const;
    [[nodiscard]] size_t chordsHash() const;
    /** The slot (bar or half bar) at x: start and length in bars. */
    [[nodiscard]] std::pair<double, double> slotAt(float x, bool forceHalf) const;
    void startEditing(double bar, double length);
    void commit(bool moveOn);
    void cancel();
    void showMenu(double bar);

    daw::ArrangementController& controller;
    const TimelineComponent& timeline;
    int labelWidth;
    std::unique_ptr<Editor> editor;
    double editBar = 0.0, editLength = 1.0;
    std::tuple<double, double, size_t, double> painted {};
};
} // namespace nam::ui
