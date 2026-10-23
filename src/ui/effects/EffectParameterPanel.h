#pragma once

#include "../../presets/PresetManager.h"
#include "../components/RotaryParameterControl.h"

#include <functional>
#include <initializer_list>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/** Background of one effect card: flat, with an accent bar while the effect is on. */
class GlassEffectSurface final : public juce::Component
{
public:
    void setActive(bool shouldBeActive) { if (active != shouldBeActive) { active = shouldBeActive; repaint(); } }
    void paint(juce::Graphics&) override;

private:
    bool active = false;
};
/** Card headers (left-aligned title with an expand chevron) and on/off switches. */
class GlassHeaderLook final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};
/** GUI editor for pre-NAM Drive, the post-amp effect rack, the compressor and the wah pedal. */
class EffectParameterPanel final : public juce::Component, private juce::Timer
{
public:
    EffectParameterPanel();
    void previewExpanded(int index);
    /** Opens one effect card and scrolls it into view (0 Drive, 1 Auto-Wah, 2 Chorus,
        3 Delay, 4 Reverb, 5 Kompressor, 6 Wah-Pedal). */
    void showCard(int index);
    ~EffectParameterPanel() override;

    void setState(const SoundState& state,
                  juce::NotificationType notification = juce::dontSendNotification);
    [[nodiscard]] SoundState state() const noexcept { return currentState; }
    /** Shows a pedal position that came from outside (mouse control) without publishing it. */
    void showWahPosition(float position);
    /** Mouse control only exists for the live rig; track chains hide the switch. */
    void setMouseWahAvailable(bool available) { mouseWahAvailable = available; resized(); }

    std::function<void(const SoundState&)> onStateChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void animateTo(int index);
    void timerCallback() override;
    double animationStart = 0;
    static constexpr int numCards = 7;
    float openness[numCards] {}, from[numCards] {};
    GlassHeaderLook headerLook;
    GlassEffectSurface surfaces[numCards];
    juce::Component reveals[numCards], parameters[numCards];
    void configureControls();
    void publish();
    void layoutEffect(juce::Rectangle<int> area,
                      std::initializer_list<RotaryParameterControl*> controls);

    SoundState currentState;
    SoundState lastPublishedState;
    void updateCardLabels();
    int expandedCard = -1; // all cards start collapsed: the list shows every effect and whether it is on
    juce::TextButton driveCard { "DRIVE" }, wahCard { "AUTO-WAH" }, chorusCard { "CHORUS" }, delayCard { "DELAY" }, reverbCard { "REVERB" }, compressorCard { "KOMPRESSOR" }, wahPedalCard { "WAH-PEDAL" };
    juce::Component cardContent;
    juce::Viewport cardViewport;
    juce::ToggleButton power[numCards];
    juce::ComboBox driveType;
    RotaryParameterControl ampPush { "AMP PUSH" };
    juce::ComboBox effectPresets[numCards];
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
    juce::ToggleButton compressorPreAmp { "VOR AMP" };
    RotaryParameterControl compressorThreshold { "THRESHOLD" }, compressorRatio { "RATIO" }, compressorAttack { "ATTACK" },
        compressorRelease { "RELEASE" }, compressorKnee { "KNEE" }, compressorMakeup { "MAKEUP" }, compressorMix { "MIX" };
    juce::ToggleButton wahMouse { "MAUS" };
    bool mouseWahAvailable = true;
    RotaryParameterControl wahPosition { "POSITION" }, wahHeel { "HEEL" }, wahToe { "TOE" }, wahPedalQ { "Q" }, wahPedalMix { "MIX" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectParameterPanel)
};
} // namespace nam::ui
