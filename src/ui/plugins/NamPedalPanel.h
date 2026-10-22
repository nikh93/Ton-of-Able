#pragma once
#include "audio/AudioEngine.h"
#include "presets/PresetManager.h"

class NamPedalPanel final : public juce::Component, private juce::Timer
{
public:
    explicit NamPedalPanel(AudioEngine&);
    void capture(SoundState&) const;
    void restore(const SoundState&);
    juce::ValueTree save();
    void restoreSaved(const juce::ValueTree&);
    void addBundledModels(const juce::File&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    void apply();
    void timerCallback() override;
    void rebuildChoices();
    juce::String resolveBundledPath(const juce::String&) const;
    AudioEngine& engine;
    juce::String path;
    juce::StringArray paths;
    juce::File bundledFolder;
    juce::Label title, hint, status, inputLabel, outputLabel;
    juce::ComboBox models;
    juce::ToggleButton enabled {"ON"};
    juce::TextButton importButton {"Pedal-NAM importieren"}, removeButton {"Entfernen"};
    juce::Slider input, output;
    std::shared_ptr<juce::FileChooser> chooser;
};
