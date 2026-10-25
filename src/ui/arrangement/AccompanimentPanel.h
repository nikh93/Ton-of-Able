#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"

namespace nam::ui
{
/** "Begleitung aus Akkorden": chooses the layers (pad, bass, shimmer) and
    their style, then creates linked tracks; for an existing accompaniment
    track it edits that track's style. */
class AccompanimentPanel final : public juce::Component
{
public:
    /** editTrack null: create new tracks, else edit this track. */
    AccompanimentPanel(daw::ArrangementController&, const juce::Uuid& editTrack, std::function<juce::ValueTree()> spaceEffect);

    std::function<void()> onDone;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void apply();
    [[nodiscard]] daw::AccompanimentStyle styleFor(daw::AccompanimentStyle::Layer) const;

    daw::ArrangementController& controller;
    juce::Uuid editTrack;
    std::function<juce::ValueTree()> spaceEffect;
    juce::Label heading, summary, rangeLabel, colourLabel, rhythmLabel;
    juce::ToggleButton pad, bass, shimmer, swell, space;
    juce::ComboBox range, colour, rhythm;
    juce::TextButton ok, cancel;
};
} // namespace nam::ui
