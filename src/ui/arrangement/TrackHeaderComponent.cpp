#include "TrackHeaderComponent.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using Theme = AmberTheme;
using daw::text;

TrackHeaderComponent::TrackHeaderComponent(daw::ArrangementController& c, const juce::Uuid& trackId)
    : controller(c), id(trackId)
{
    name.setEditable(false, true, false);
    name.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    name.setColour(juce::Label::textColourId, Theme::text());
    name.setTooltip("Doppelklick zum Umbenennen");
    name.onTextChange = [this]
    {
        const auto newName = name.getText().trim();
        if (newName.isEmpty())
            return;
        controller.perform("Spur umbenennen", [&](daw::ArrangementState& state)
        {
            if (auto* track = state.findTrack(id))
                track->name = newName;
        });
    };
    name.addMouseListener(this, false);
    addAndMakeVisible(name);

    arm.setClickingTogglesState(false);
    arm.setTooltip(text("Aufnahme scharf schalten"));
    arm.setColour(juce::TextButton::textColourOffId, Theme::danger().withAlpha(0.7f));
    arm.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    arm.onClick = [this]
    {
        controller.modify([this](daw::ArrangementState& state)
        {
            if (auto* track = state.findTrack(id))
                track->armed = ! track->armed;
        });
    };
    mute.setTooltip("Stumm");
    mute.onClick = [this] { if (mixer) { mixer->muted = ! mixer->muted.load(); controller.markDirty(); repaint(); } };
    solo.setTooltip("Solo");
    solo.onClick = [this] { if (mixer) { mixer->soloed = ! mixer->soloed.load(); controller.markDirty(); repaint(); } };
    monitor.setTooltip(text("Eingang abhören, solange die Spur scharf ist (nur dry-Eingänge; das Rig ist immer hörbar)"));
    monitor.onClick = [this]
    {
        controller.modify([this](daw::ArrangementState& state)
        {
            if (auto* track = state.findTrack(id))
                track->monitorInput = ! track->monitorInput;
        });
    };
    fx.setTooltip(text("Eigene Effektkette dieser Spur: NAM, IR, EQ, Graphic EQ, Effekte – Insert oder Print"));
    fx.onClick = [this] { if (onOpenFx) onOpenFx(id); };
    for (auto* button : { &arm, &mute, &solo, &monitor, &fx })
        addAndMakeVisible(*button);

    input.setTooltip(text("Aufnahmequelle: NAM-Rig (bearbeitet) oder ein trockener Eingang des Interfaces"));
    input.onChange = [this]
    {
        const auto index = input.getSelectedItemIndex();
        if (index < 0 || index >= static_cast<int>(inputChannels.size()))
            return;
        const auto channel = inputChannels[static_cast<size_t>(index)];
        controller.modify([this, channel](daw::ArrangementState& state)
        {
            if (auto* track = state.findTrack(id))
                track->inputChannel = channel;
        });
    };
    addAndMakeVisible(input);

    volume.setRange(-60.0, 6.0, 0.1);
    volume.setSkewFactorFromMidPoint(-12.0);
    volume.setDoubleClickReturnValue(true, 0.0);
    volume.setPopupDisplayEnabled(true, true, nullptr);
    volume.textFromValueFunction = [](double v) { return v <= -59.9 ? juce::String("-inf dB") : juce::String(v, 1) + " dB"; };
    volume.onValueChange = [this] { if (mixer) { mixer->volumeDb = static_cast<float>(volume.getValue()); controller.markDirty(); } };
    addAndMakeVisible(volume);

    pan.setRange(-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue(true, 0.0);
    pan.setPopupDisplayEnabled(true, true, nullptr);
    pan.textFromValueFunction = [](double v)
    {
        if (std::abs(v) < 0.005) return juce::String("C");
        return juce::String(std::abs(v) * 50.0, 0) + (v < 0 ? "L" : "R");
    };
    pan.onValueChange = [this] { if (mixer) { mixer->pan = static_cast<float>(pan.getValue()); controller.markDirty(); } };
    addAndMakeVisible(pan);
}

void TrackHeaderComponent::refresh(const daw::Track& track, const std::vector<daw::ArrangementController::InputChoice>& inputs, bool selected)
{
    mixer = track.mixer;
    colour = track.colour;
    isSelected = selected;
    if (! name.isBeingEdited())
        name.setText(track.name, juce::dontSendNotification);
    arm.setToggleState(track.armed, juce::dontSendNotification);
    arm.setColour(juce::TextButton::buttonOnColourId, Theme::danger());
    monitor.setToggleState(track.monitorInput, juce::dontSendNotification);
    monitor.setEnabled(track.inputChannel >= 0);
    fx.setToggleState(track.fx->enabled.load(), juce::dontSendNotification);
    fx.setButtonText(track.fx->enabled.load() && track.fx->printWhileRecording.load() && track.inputChannel >= 0 ? "FX P" : "FX");
    mute.setToggleState(track.mixer->muted.load(), juce::dontSendNotification);
    solo.setToggleState(track.mixer->soloed.load(), juce::dontSendNotification);
    if (! volume.isMouseButtonDown())
        volume.setValue(track.mixer->volumeDb.load(), juce::dontSendNotification);
    if (! pan.isMouseButtonDown())
        pan.setValue(track.mixer->pan.load(), juce::dontSendNotification);

    std::vector<int> channels;
    for (const auto& choice : inputs)
        channels.push_back(choice.channel);
    if (channels != inputChannels || input.getNumItems() != static_cast<int>(inputs.size()))
    {
        inputChannels = channels;
        input.clear(juce::dontSendNotification);
        for (size_t index = 0; index < inputs.size(); ++index)
            input.addItem(inputs[index].name, static_cast<int>(index) + 1);
    }
    const auto found = std::find(inputChannels.begin(), inputChannels.end(), track.inputChannel);
    if (found != inputChannels.end())
        input.setSelectedItemIndex(static_cast<int>(found - inputChannels.begin()), juce::dontSendNotification);
    else
        input.setText("In " + juce::String(track.inputChannel + 1) + " (nicht aktiv)", juce::dontSendNotification);
    repaint();
}

void TrackHeaderComponent::updateMeter()
{
    if (mixer == nullptr)
        return;
    const auto peak = mixer->meter.exchange(0.0f);
    const auto level = juce::jlimit(0.0f, 1.0f, juce::jmap(juce::Decibels::gainToDecibels(peak, -60.0f), -60.0f, 0.0f, 0.0f, 1.0f));
    const auto next = std::max(level, meterLevel - 0.06f);
    if (std::abs(next - meterLevel) > 0.002f)
    {
        meterLevel = next;
        repaint(getLocalBounds().removeFromRight(8));
    }
}

void TrackHeaderComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(isSelected ? Theme::surfaceRaised().brighter(0.08f) : Theme::surface());
    g.fillRect(bounds);
    g.setColour(colour);
    g.fillRect(bounds.removeFromLeft(5.0f));
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

    auto meter = getLocalBounds().removeFromRight(8).reduced(2, 4).toFloat();
    g.setColour(juce::Colour(0xff15110d));
    g.fillRect(meter);
    const auto height = meter.getHeight() * meterLevel;
    g.setColour(meterLevel > 0.97f ? Theme::danger() : Theme::amber());
    g.fillRect(meter.removeFromBottom(height));
}

void TrackHeaderComponent::resized()
{
    auto area = getLocalBounds().withTrimmedLeft(10).withTrimmedRight(10).reduced(0, 4);
    auto top = area.removeFromTop(22);
    solo.setBounds(top.removeFromRight(24));
    top.removeFromRight(2);
    mute.setBounds(top.removeFromRight(24));
    top.removeFromRight(2);
    arm.setBounds(top.removeFromRight(26));
    top.removeFromRight(4);
    name.setBounds(top);

    const auto roomy = area.getHeight() >= 46;
    input.setVisible(roomy);
    fx.setVisible(roomy);
    monitor.setVisible(roomy);
    volume.setVisible(roomy);
    pan.setVisible(roomy);
    if (! roomy)
        return;
    area.removeFromTop(4);
    auto second = area.removeFromTop(20);
    monitor.setBounds(second.removeFromRight(30));
    second.removeFromRight(2);
    fx.setBounds(second.removeFromRight(36));
    second.removeFromRight(4);
    input.setBounds(second);
    area.removeFromTop(2);
    auto third = area.removeFromTop(22);
    pan.setBounds(third.removeFromRight(26));
    volume.setBounds(third);
}

void TrackHeaderComponent::mouseDown(const juce::MouseEvent& event)
{
    if (onSelect)
        onSelect(id);
    if (event.mods.isPopupMenu())
        showMenu();
}

void TrackHeaderComponent::rename()
{
    name.showEditor();
}

void TrackHeaderComponent::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Umbenennen");
    menu.addItem(2, "Farbe wechseln");
    menu.addItem(3, text("Spur exportieren …"));
    menu.addItem(5, text("Spur-Effekte …"));
    menu.addSeparator();
    menu.addItem(4, text("Spur löschen"));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe = juce::Component::SafePointer<TrackHeaderComponent>(this)](int result)
    {
        if (safe == nullptr || result == 0)
            return;
        auto& self = *safe;
        const auto trackId = self.id;
        switch (result)
        {
            case 1: self.rename(); break;
            case 2:
                self.controller.perform("Spurfarbe", [trackId](daw::ArrangementState& state)
                {
                    if (auto* track = state.findTrack(trackId))
                        track->colour = track->colour.withRotatedHue(0.13f);
                });
                break;
            case 3: if (self.onExportTrack) self.onExportTrack(trackId); break;
            case 4: self.controller.removeTrack(trackId); break; // destroys this component
            case 5: if (self.onOpenFx) self.onOpenFx(trackId); break;
            default: break;
        }
    });
}
} // namespace nam::ui
