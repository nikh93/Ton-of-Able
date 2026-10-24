#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"
#include "TimelineComponent.h"

namespace nam::ui
{
/** The strip below the tracks that sets what an export renders: drag its
    start or end, drag the middle to move it, double-click for the whole song. */
class ExportRangeBar final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ExportRangeBar(daw::ArrangementController&, const TimelineComponent&, int labelWidth);
    /** Repaints when the view or the range changed (called from the view's timer). */
    void update();

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    enum class Part { none, start, end, body };
    [[nodiscard]] float toX(juce::int64 sample) const;
    [[nodiscard]] juce::int64 toSample(float x) const;
    [[nodiscard]] Part partAt(float x) const;

    daw::ArrangementController& controller;
    const TimelineComponent& timeline;
    int labelWidth;
    Part dragging = Part::none;
    juce::int64 anchor = 0, startAtDown = 0, endAtDown = 0;
    std::tuple<double, double, juce::int64, juce::int64, juce::int64> painted {};
};
} // namespace nam::ui
