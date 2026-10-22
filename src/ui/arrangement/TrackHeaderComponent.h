#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"

namespace nam::ui
{
/** Track controls left of the timeline: name, record arm, mute, solo,
    input monitoring, input selection, volume, pan and a level meter. */
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

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void showMenu();
    void rename();

    daw::ArrangementController& controller;
    juce::Uuid id;
    juce::Colour colour;
    bool isSelected = false;
    float meterLevel = 0.0f;
    std::shared_ptr<daw::TrackMixer> mixer;
    juce::Label name;
    juce::TextButton arm { juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) }, mute { "M" }, solo { "S" }, monitor { "IN" }, fx { "FX" };
    juce::ComboBox input;
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider pan { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    std::vector<int> inputChannels;
};
} // namespace nam::ui
