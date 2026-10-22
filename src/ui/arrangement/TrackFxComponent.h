#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"
#include "presets/PresetManager.h"
#include "ui/components/RotaryParameterControl.h"
#include "ui/effects/EffectParameterPanel.h"
#include "ui/effects/GraphicEqComponent.h"
#include "ui/effects/PitchShiftComponent.h"

namespace nam::ui
{
/** Editor for one track's own effect chain (NAM pedal/amp, IR, amp EQ,
    graphic EQ, Drive/Wah/Chorus/Delay/Reverb, VST3 from the live rig) and
    whether it is printed while recording or inserted on playback. */
class TrackFxComponent final : public juce::Component
{
public:
    struct Choice
    {
        juce::String name;
        juce::String id;    // library id (NAM/IR) or file path (pedal)
    };

    struct Sources
    {
        std::vector<Choice> amps, irs, pedals;
        std::function<SoundState()> captureLiveRig;
    };

    TrackFxComponent(daw::ArrangementController&, const juce::Uuid& trackId, Sources);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void loadFromTrack();
    void publish();
    void showState();
    static void fill(juce::ComboBox&, const std::vector<Choice>&, const juce::String& none);
    static void select(juce::ComboBox&, const std::vector<Choice>&, const juce::String& id);

    daw::ArrangementController& controller;
    juce::Uuid trackId;
    Sources sources;
    SoundState sound;
    bool enabled = false;
    bool print = false;

    juce::Label title;
    juce::ToggleButton active { "Effekte aktiv" };
    juce::TextButton insertMode, printMode; // texts set in the constructor (UTF-8)
    juce::TextButton fromLiveRig, clearVst { "VST3 entfernen" };
    juce::ComboBox amp, ir, pedal;
    juce::ToggleButton namOn { "NAM" }, irOn { "IR" };
    juce::Label vstInfo, modeInfo;
    RotaryParameterControl inputGain { "INPUT" }, bass { "BASS" }, mid { "MID" }, treble { "TREBLE" }, outputGain { "OUTPUT" };
    PitchShiftComponent pitch;
    GraphicEqComponent graphicEq;
    EffectParameterPanel effects;
};
} // namespace nam::ui
