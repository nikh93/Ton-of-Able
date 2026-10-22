#include "PresetManagementComponent.h"

#include "../components/AmberTheme.h"

namespace nam::ui
{
namespace
{
constexpr int firstPresetId = 1;
}

PresetManagementComponent::PresetManagementComponent()
{
    addAndMakeVisible(presetSelector);
    presetSelector.setTextWhenNothingSelected("Select preset");
    presetSelector.onChange = [this] { choosePreset(presetSelector.getSelectedId()); };

    for (auto* button : { &saveButton, &saveAsButton, &loadButton, &renameButton, &deleteButton, &aButton, &bButton })
    {
        addAndMakeVisible(*button);
        button->setColour(juce::TextButton::buttonColourId, AmberTheme::surfaceRaised());
        button->setColour(juce::TextButton::textColourOffId, AmberTheme::text());
    }

    saveButton.onClick = [this] { if (! selectedPreset.isNull() && onSave) onSave(selectedPreset); };
    saveAsButton.onClick = [this] {
        requestName("Save preset as", "New Preset", [this] (juce::String name) { if (onSaveAs) onSaveAs(std::move(name)); });
    };
    loadButton.onClick = [this] { if (! selectedPreset.isNull() && onLoad) onLoad(selectedPreset); };
    renameButton.onClick = [this] {
        if (const auto* preset = presetManager != nullptr ? presetManager->find(selectedPreset) : nullptr)
            requestName("Rename preset", preset->name, [this] (juce::String name) { if (onRename) onRename(selectedPreset, std::move(name)); });
    };
    deleteButton.onClick = [this] { requestDeleteConfirmation(); };
    aButton.onClick = [this] { setUsingA(true, juce::sendNotificationSync); };
    bButton.onClick = [this] { setUsingA(false, juce::sendNotificationSync); };
    updateControlState();
}

void PresetManagementComponent::setPresetManager(const PresetManager* manager)
{
    presetManager = manager;
    selectedPreset = {};
    refresh();
}

void PresetManagementComponent::refresh()
{
    presetSelector.clear(juce::dontSendNotification);
    if (presetManager != nullptr)
    {
        int id = firstPresetId;
        for (const auto& preset : presetManager->presets())
        {
            presetSelector.addItem(preset.name, id++);
            if (preset.id == selectedPreset)
                presetSelector.setSelectedId(id - 1, juce::dontSendNotification);
        }
    }
    updateControlState();
}

void PresetManagementComponent::setSelectedPreset(const juce::Uuid& id)
{
    selectedPreset = id;
    refresh();
}

void PresetManagementComponent::setUsingA(bool shouldUseA, juce::NotificationType notification)
{
    const bool changed = usingA != shouldUseA;
    usingA = shouldUseA;
    aButton.setToggleState(usingA, juce::dontSendNotification);
    bButton.setToggleState(!usingA, juce::dontSendNotification);
    aButton.setColour(juce::TextButton::textColourOnId, AmberTheme::amberBright());
    bButton.setColour(juce::TextButton::textColourOnId, AmberTheme::amberBright());
    aButton.setColour(juce::TextButton::buttonColourId, usingA ? AmberTheme::amber() : AmberTheme::surfaceRaised());
    bButton.setColour(juce::TextButton::buttonColourId, usingA ? AmberTheme::surfaceRaised() : AmberTheme::amber());
    aButton.setColour(juce::TextButton::textColourOffId, usingA ? AmberTheme::background() : AmberTheme::text());
    bButton.setColour(juce::TextButton::textColourOffId, usingA ? AmberTheme::text() : AmberTheme::background());
    if (changed && notification != juce::dontSendNotification && onABSelectionChanged)
        onABSelectionChanged(usingA);
}

void PresetManagementComponent::choosePreset(int comboBoxId)
{
    if (presetManager == nullptr || comboBoxId < firstPresetId)
        return;
    const int index = comboBoxId - firstPresetId;
    if (juce::isPositiveAndBelow(index, presetManager->presets().size()))
        selectedPreset = presetManager->presets().getReference(index).id;
    updateControlState();
}

void PresetManagementComponent::updateControlState()
{
    const bool validSelection = presetManager != nullptr && presetManager->find(selectedPreset) != nullptr;
    saveButton.setEnabled(validSelection && static_cast<bool>(onSave));
    loadButton.setEnabled(validSelection && static_cast<bool>(onLoad));
    renameButton.setEnabled(validSelection && static_cast<bool>(onRename));
    deleteButton.setEnabled(validSelection && static_cast<bool>(onDelete));
    saveAsButton.setEnabled(static_cast<bool>(onSaveAs));
    setUsingA(usingA);
}

void PresetManagementComponent::requestName(const juce::String& title, const juce::String& initial,
                                             std::function<void(juce::String)> completion)
{
    auto* dialog = new juce::AlertWindow(title, "Preset name:", juce::MessageBoxIconType::QuestionIcon);
    dialog->addTextEditor("name", initial, "Name");
    dialog->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [dialog, completion = std::move(completion)] (int result)
            {
                const auto name = dialog->getTextEditorContents("name").trim();
                delete dialog;
                if (result == 1 && completion)
                    completion(name);
            }),
        false);
}

void PresetManagementComponent::requestDeleteConfirmation()
{
    if (selectedPreset.isNull() || ! onDelete) return;
    const auto id = selectedPreset;
    const juce::Component::SafePointer<PresetManagementComponent> self(this);
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Delete preset").withMessage("Delete the selected preset?").withButton("Delete").withButton("Cancel"),
        [self, id] (int result) { if (self != nullptr && result == 1 && self->onDelete) self->onDelete(id); });
}

void PresetManagementComponent::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3e433c), 0, 0,
        juce::Colour(0xff232822), 0, float(getHeight()), false));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    g.setColour(juce::Colours::white.withAlpha(.12f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f), 6, 1);
    g.setColour(AmberTheme::subduedText());
    g.setFont(13.0f);
    g.drawText("PRESETS", 10, 6, 120, 20, juce::Justification::centredLeft);
    g.drawText("A / B", getWidth() - 80, 6, 70, 20, juce::Justification::centred);
}

void PresetManagementComponent::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(22);
    auto top = area.removeFromTop(28);
    presetSelector.setBounds(top.removeFromLeft(juce::jmax(120, top.getWidth() - 240)));
    top.removeFromLeft(6); saveButton.setBounds(top.removeFromLeft(52)); top.removeFromLeft(4);
    saveAsButton.setBounds(top.removeFromLeft(70)); top.removeFromLeft(4);
    loadButton.setBounds(top.removeFromLeft(52));
    area.removeFromTop(6);
    auto bottom = area.removeFromTop(28);
    renameButton.setBounds(bottom.removeFromLeft(70)); bottom.removeFromLeft(4);
    deleteButton.setBounds(bottom.removeFromLeft(65));
    bottom.removeFromLeft(juce::jmax(4, bottom.getWidth() - 58));
    aButton.setBounds(bottom.removeFromLeft(27)); bottom.removeFromLeft(4); bButton.setBounds(bottom.removeFromLeft(27));
}
} // namespace nam::ui
