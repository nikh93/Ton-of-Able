#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "audio/plugins/GuitarSynth.h"
#include "ui/components/Icons.h"

namespace nam::ui
{
/** Editor for the guitar-to-MIDI synth: the tracker plugin that turns the
    guitar into notes, the instrument that plays them, levels, transpose,
    MIDI keyboards and a live note display. */
class GuitarSynthPanel final : public juce::Component, private juce::Timer
{
public:
    struct Host
    {
        /** All scanned VST3 plugins (instruments and effects). */
        std::function<juce::Array<juce::PluginDescription>()> plugins;
        std::function<void(std::function<void()> done)> rescan;
        std::function<void(const juce::String&)> status;
        /** Called after any change (the signal chain shows the state). */
        std::function<void()> changed;
    };

    GuitarSynthPanel(GuitarSynth&, Host);
    ~GuitarSynthPanel() override;

    /** Closes the plugin windows; required before the plugins go away. */
    void closeEditors();
    void refresh();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void choose(GuitarSynth::Slot, int comboId);
    void openEditor(GuitarSynth::Slot);
    void fillLists();
    [[nodiscard]] juce::String noteName(int note) const;

    GuitarSynth& synth;
    Host host;
    juce::Array<juce::PluginDescription> trackers, instruments;

    juce::ToggleButton power;
    juce::Label title, trackerLabel, synthLabel, mixLabel, hint, scanInfo;
    juce::ComboBox trackerBox, synthBox, transpose;
    IconButton trackerEditor { Icon::sliders, "Editor" }, synthEditor { Icon::sliders, "Editor" };
    IconButton trackerRemove { Icon::none, "Entfernen" }, synthRemove { Icon::none, "Entfernen" };
    IconButton scan { Icon::folder, "VST3-Plugins suchen" };
    juce::Label synthLevelLabel { {}, "Synth" }, guitarLevelLabel { {}, "Gitarre" }, transposeLabel { {}, "Transponieren" };
    juce::Slider synthLevel { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider guitarLevel { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton keyboards { "MIDI-Keyboards spielen den Synth mit" };
    std::unique_ptr<juce::DocumentWindow> editors[2];
    juce::uint32 lastCount = 0;
    float flash = 0.0f;
    juce::Rectangle<int> noteArea;
};
} // namespace nam::ui
