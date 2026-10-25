#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"
#include "ui/components/Icons.h"

namespace nam::ui
{
/** Track controls left of the timeline: name, record arm, mute, solo,
    input monitoring, input selection, sends, volume, pan and a level meter.
    Return tracks (send effects) show their slot instead of input and sends;
    frozen tracks show a snowflake that thaws them. */
class TrackHeaderComponent final : public juce::Component
{
public:
    TrackHeaderComponent(daw::ArrangementController&, const juce::Uuid& trackId);

    void refresh(const daw::Track&, const std::vector<daw::ArrangementController::InputChoice>&, bool selected);
    void updateMeter();
    [[nodiscard]] const juce::Uuid& trackId() const noexcept { return id; }

    std::function<void(const juce::Uuid&)> onSelect;
    std::function<void(const juce::Uuid&)> onExportTrack;
    std::function<void(const juce::Uuid&)> onOpenFx;
    std::function<void(const juce::Uuid&)> onOpenDrums;
    /** MIDI tracks: the instrument menu under the instrument button. */
    std::function<void(const juce::Uuid&, juce::Component& target)> onInstrumentMenu;
    /** Accompaniment tracks: edit the style of the generated part. */
    std::function<void(const juce::Uuid&)> onEditAccompaniment;
    /** Drum tracks: the drum arrangement editor. */
    std::function<void(const juce::Uuid&)> onOpenDrumArrangement;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void showMenu();
    void rename();

public:
    /** Opens the send levels of this track next to its SEND button. */
    void showSends();

private:

    daw::ArrangementController& controller;
    juce::Uuid id;
    juce::Colour colour;
    bool isSelected = false;
    float meterLevel = 0.0f;
    std::shared_ptr<daw::TrackMixer> mixer;
    juce::Label name;
    IconButton arm { Icon::record }, monitor { Icon::headphones }, frozenBadge { Icon::snowflake };
    juce::TextButton mute { "M" }, solo { "S" }, fx { "FX" }, send { "SEND" }, reampBadge { "RE-AMP" }, chordBadge;
    IconButton drums { Icon::sliders, "Drumcomputer" };
    IconButton instrumentButton { Icon::keyboard, "Synth" };
    juce::Label role;
    juce::ComboBox input;
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider pan { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    std::vector<int> inputChannels;
    bool isDrumTrack = false;
    bool isReturnTrack = false;
    bool isMidiTrack = false;
    bool frozen = false, freezing = false, reamping = false, linkedToChords = false, generated = false;
};
} // namespace nam::ui
