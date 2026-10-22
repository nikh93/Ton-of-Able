#pragma once
#include "audio/plugins/Vst3Slot.h"
#include "NamPedalPanel.h"
#include <juce_gui_extra/juce_gui_extra.h>

class PluginRackPanel final : public juce::Component, private juce::Timer
{
public:
    explicit PluginRackPanel(AudioEngine&);
    void capturePedal(SoundState& s) const { pedal.capture(s); }
    void restorePedal(const SoundState& s) { pedal.restore(s); }
    void addBundledPedals(const juce::File& folder) { pedal.addBundledModels(folder); }
    ~PluginRackPanel() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void closeEditors();
    void refresh();
    void scanInstalled(bool loadDefaults = false);
    void restore(const juce::ValueTree& pre, const juce::ValueTree& post);
    juce::ValueTree saveRack();
    void restoreRack(const juce::ValueTree&);
    std::function<void(const juce::String&)> onStatus;
private:
    void timerCallback() override;
    void choosePlugin();
    void scan(const juce::Array<juce::File>& files, bool loadDefaults);
    void openEditor(int slot);
    Vst3Slot& slot(int index) { return index == 0 ? preSlot : postSlot; }
    Vst3Slot& preSlot;
    Vst3Slot& postSlot;
    NamPedalPanel pedal;
    juce::Label heading, route, names[2], status;
    juce::ComboBox choices[2], programs[2];
    juce::ToggleButton enabled[2];
    juce::TextButton editors[2], remove[2], importButton {"VST3 importieren"}, scanButton {"Plugin-Ordner scannen"};
    juce::Array<juce::PluginDescription> descriptions;
    std::unique_ptr<juce::DocumentWindow> windows[2];
    std::shared_ptr<juce::FileChooser> chooser;
    juce::ThreadPool scanPool {1};
    bool scanning = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRackPanel)
};
