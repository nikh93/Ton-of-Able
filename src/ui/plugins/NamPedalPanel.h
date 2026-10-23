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
    /** For the signal chain view: a pedal model is selected and switched on. */
    [[nodiscard]] bool isActive() const { return path.isNotEmpty() && enabled.getToggleState(); }
    [[nodiscard]] bool hasModel() const { return path.isNotEmpty(); }
    [[nodiscard]] juce::String modelName() const { return juce::File(path).getFileNameWithoutExtension(); }
    void setActive(bool active) { enabled.setToggleState(active, juce::dontSendNotification); apply(); }
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
