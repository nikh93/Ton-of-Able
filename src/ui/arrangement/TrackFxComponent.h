#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"
#include "presets/PresetManager.h"
#include "ui/components/RotaryParameterControl.h"
#include "ui/effects/EffectParameterPanel.h"
#include "ui/effects/GraphicEqComponent.h"
#include "ui/effects/PitchShiftComponent.h"
#include "ui/rig/SignalChainStrip.h"

class RigChain;

namespace nam::ui
{
/** Editor for one track's own effect chain: the same editable signal chain
    as the live rig (order, VST3 plugins, routing), NAM pedal/amp, IR, amp EQ,
    graphic EQ and effects, and whether it is printed while recording or
    inserted on playback. */
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
        std::function<juce::Array<juce::PluginDescription>()> plugins;
        std::function<void(std::function<void()> done)> rescanPlugins;
    };

    TrackFxComponent(daw::ArrangementController&, const juce::Uuid& trackId, Sources);
    ~TrackFxComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void loadFromTrack();
    void publish();
    void showState();
    static void fill(juce::ComboBox&, const std::vector<Choice>&, const juce::String& none);
    /** The track's running chain (for plugin windows and states), or null. */
    [[nodiscard]] RigChain* trackChain() const;
    [[nodiscard]] static juce::String nameOf(const std::vector<Choice>&, const juce::String& id);
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
    juce::TextButton fromLiveRig;
    juce::ComboBox amp, ir, pedal;
    juce::ToggleButton namOn { "NAM" }, irOn { "IR" };
    juce::Label modeInfo;
    SignalChainStrip chainStrip;
    juce::Viewport chainViewport;
    std::map<juce::String, std::unique_ptr<juce::DocumentWindow>> pluginWindows;
    RotaryParameterControl inputGain { "INPUT" }, bass { "BASS" }, mid { "MID" }, treble { "TREBLE" }, outputGain { "OUTPUT" };
    PitchShiftComponent pitch;
    GraphicEqComponent graphicEq;
    EffectParameterPanel effects;
};
} // namespace nam::ui
