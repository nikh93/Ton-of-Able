#pragma once

#include <map>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"

namespace nam::ui
{
/** Step sequencer for a drum track: eight patterns of up to 64 sixteenths,
    twelve General MIDI drum lanes, swing, audition loop, sound source (built-in
    kit or a VST3 instrument such as JDrummer) and insertion as pattern clips. */
class DrumMachineComponent final : public juce::Component, private juce::Timer
{
public:
    struct Host
    {
        std::function<juce::Array<juce::PluginDescription>()> instruments;
        std::function<void(std::function<void()> done)> rescan;
        /** null description = built-in kit */
        std::function<juce::Result(const juce::Uuid& trackId, const juce::PluginDescription*)> setSource;
        std::function<void(const juce::Uuid& trackId)> openEditor;
        /** Selects a bundled sample kit; an empty id selects the synthesized kit. */
        std::function<juce::Result(const juce::Uuid& trackId, const juce::String& kitId)> setKit;
        /** The drum arrangement editor of this track (sections, groove library, auto mode). */
        std::function<void(const juce::Uuid& trackId)> openArrangement;
    };

    DrumMachineComponent(daw::ArrangementController&, const juce::Uuid& trackId, int pattern, Host);
    ~DrumMachineComponent() override;

    void selectPattern(int index);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void refreshSources();
    void refreshControls();
    void refreshMorePatterns();
    [[nodiscard]] const daw::Track* track() const;
    [[nodiscard]] const daw::DrumPattern* pattern() const;
    [[nodiscard]] juce::Rectangle<int> cellBounds(int lane, int step) const;
    [[nodiscard]] bool cellAt(juce::Point<int>, int& lane, int& step) const;
    void editPattern(const juce::String& description, const std::function<void(daw::DrumPattern&)>& change);
    void applyPainted(daw::ArrangementState&) const;
    [[nodiscard]] int currentStep() const;

    daw::ArrangementController& controller;
    juce::Uuid trackId;
    Host host;
    int selected = 0;
    int playingStep = -1;
    bool auditioning = false;

    juce::Label title, info;
    juce::ComboBox source;
    juce::TextButton editor { "Plugin-Editor" }, arrangement;
    juce::ComboBox morePatterns;   // patterns beyond A-H (e.g. from the groove library)
    int morePatternCount = -1;
    juce::OwnedArray<juce::TextButton> patternButtons;
    juce::ComboBox length;
    juce::Label swingLabel { {}, "Swing" };
    juce::Slider swing { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton audition, clear { "Leeren" };
    juce::ComboBox bars;
    juce::TextButton insert;
    juce::Array<juce::PluginDescription> instrumentList;

    juce::Rectangle<int> gridArea, laneArea;
    std::map<std::pair<int, int>, juce::uint8> painted;
    juce::uint8 paintVelocity = 0;
    int lastLane = 0, lastStep = 0;
    bool painting = false;
};
} // namespace nam::ui
