#pragma once

#include "../../presets/PresetManager.h"
#include "../components/RotaryParameterControl.h"

#include <functional>
#include <initializer_list>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
class GlassEffectSurface final : public juce::Component
{
public:
    void paint(juce::Graphics&) override;
};
class GlassHeaderLook final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};
/** GUI editor for pre-NAM Drive and the post-amp effect rack. */
class EffectParameterPanel final : public juce::Component, private juce::Timer
{
public:
    EffectParameterPanel();
    void previewExpanded(int index);
    ~EffectParameterPanel() override;

    void setState(const SoundState& state,
                  juce::NotificationType notification = juce::dontSendNotification);
    [[nodiscard]] SoundState state() const noexcept { return currentState; }

    std::function<void(const SoundState&)> onStateChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void animateTo(int index);
    void timerCallback() override;
    double animationStart = 0;
    float openness[5] { 0, 1, 0, 0, 0 }, from[5] {};
    GlassHeaderLook headerLook;
    GlassEffectSurface surfaces[5];
    juce::Component reveals[5], parameters[5];
    void configureControls();
    void publish();
    void layoutEffect(juce::Rectangle<int> area,
                      std::initializer_list<RotaryParameterControl*> controls);

    SoundState currentState;
    SoundState lastPublishedState;
    void updateCardLabels();
    int expandedCard = 1;
    juce::TextButton driveCard { "DRIVE" }, wahCard { "AUTO-WAH" }, chorusCard { "CHORUS" }, delayCard { "DELAY" }, reverbCard { "REVERB" };
    juce::Component cardContent;
    juce::Viewport cardViewport;
    juce::ToggleButton power[5];
    juce::ComboBox driveType;
    RotaryParameterControl ampPush { "AMP PUSH" };
    juce::ComboBox effectPresets[5];
    void applyEffectPreset(int effect, int preset);
    RotaryParameterControl driveAmount { "DRIVE" }, driveTone { "TONE" }, driveCharacter { "WARMTH" }, driveSustain { "SUSTAIN" }, driveLevel { "LEVEL" }, driveMix { "MIX" };
    juce::ToggleButton wahDirection { "UP" };
    RotaryParameterControl wahSensitivity { "SENS" }, wahAttack { "ATTACK" }, wahRelease { "RELEASE" },
        wahMin { "MIN Hz" }, wahMax { "MAX Hz" }, wahQ { "Q" }, wahMix { "MIX" };
    RotaryParameterControl chorusRate { "RATE" }, chorusDepth { "DEPTH" }, chorusDelay { "DELAY" },
        chorusFeedback { "FDBK" }, chorusMix { "MIX" };
    RotaryParameterControl delayTime { "TIME" }, delayFeedback { "FDBK" }, delayMix { "MIX" },
        delayLowCut { "LOW CUT" }, delayHighCut { "HIGH CUT" };
    RotaryParameterControl reverbSize { "SIZE" }, reverbDecay { "DECAY" }, reverbDamping { "DAMP" },
        reverbPreDelay { "PRE DELAY" }, reverbMix { "MIX" }, reverbLowCut { "LOW CUT" }, reverbHighCut { "HIGH CUT" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectParameterPanel)
};
} // namespace nam::ui
