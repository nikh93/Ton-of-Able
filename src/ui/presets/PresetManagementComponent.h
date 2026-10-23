#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../presets/PresetManager.h"

namespace nam::ui
{
/**
    Reusable message-thread UI for a non-owning PresetManager. The component
    never alters sound state itself: its callbacks let the application prepare
    and publish changes safely outside the audio callback.
*/
class PresetManagementComponent final : public juce::Component
{
public:
    PresetManagementComponent();

    void setPresetManager(const PresetManager* manager);
    [[nodiscard]] const PresetManager* getPresetManager() const noexcept { return presetManager; }
    void refresh();

    void setSelectedPreset(const juce::Uuid& id);
    [[nodiscard]] juce::Uuid getSelectedPreset() const noexcept { return selectedPreset; }

    void setUsingA(bool shouldUseA, juce::NotificationType notification = juce::dontSendNotification);
    [[nodiscard]] bool isUsingA() const noexcept { return usingA; }

    std::function<void(const juce::Uuid&)> onSave;
    std::function<void(juce::String)> onSaveAs;
    std::function<void(const juce::Uuid&)> onLoad;
    std::function<void(const juce::Uuid&, juce::String)> onRename;
    std::function<void(const juce::Uuid&)> onDelete;
    std::function<void(bool useA)> onABSelectionChanged;

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void updateControlState();
    void choosePreset(int comboBoxId);
    void requestName(const juce::String& title, const juce::String& initial,
                     std::function<void(juce::String)> completion);
    void requestDeleteConfirmation();

    const PresetManager* presetManager = nullptr;
    juce::Uuid selectedPreset;
    bool usingA = true;

    juce::ComboBox presetSelector;
    juce::TextButton saveButton { "Speichern" }, saveAsButton;
    juce::TextButton loadButton { "Laden" }, renameButton { "Umbenennen" }, deleteButton;
    juce::TextButton aButton { "A" }, bButton { "B" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManagementComponent)
};
} // namespace nam::ui
