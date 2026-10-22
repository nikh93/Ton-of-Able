#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/** Controls for the Rubber Band live pitch shifter: on/off, semitones with
    quick buttons (octave down etc.), mix, formants and latency mode. */
class PitchShiftComponent final : public juce::Component, private juce::Timer
{
public:
    struct Settings
    {
        bool bypassed = true;
        float semitones = -12.0f;
        float mix = 1.0f;
        bool formantPreserved = false;
        bool highQuality = false;
    };

    PitchShiftComponent();
    void setSettings(const Settings&);
    [[nodiscard]] Settings settings() const noexcept { return current; }

    std::function<void(const Settings&)> onChange;
    /** Optional: returns the current processing delay in ms for the info line. */
    std::function<double()> latencyMs;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void publish();
    void refresh();

    Settings current;
    juce::ToggleButton power;
    juce::Slider semitones { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider mix { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label semitoneLabel, mixLabel, info;
    juce::ToggleButton formant;
    juce::ComboBox mode;
    juce::OwnedArray<juce::TextButton> quick;
};
} // namespace nam::ui
