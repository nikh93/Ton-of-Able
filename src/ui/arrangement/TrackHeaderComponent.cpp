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
    drums.setTooltip(text("Step-Sequencer und Klangquelle dieser Drum-Spur öffnen"));
    drums.onClick = [this] { if (onOpenDrums) onOpenDrums(id); };
    addChildComponent(drums);
    for (auto* button : { &arm, &mute, &solo, &monitor, &fx })
        addAndMakeVisible(*button);
    send.setTooltip(text("Sends: wie viel dieser Spur in die Return-Spuren (Hall, Delay …) geht"));
    send.onClick = [this] { showSends(); };
    addAndMakeVisible(send);
    frozenBadge.setTooltip(text("Eingefroren: spielt eine vorberechnete Datei statt Effekten und Instrument. Klick zum Auftauen."));
    frozenBadge.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff9fd8ff));
    frozenBadge.onClick = [this] { if (frozen) controller.unfreezeTrack(id); };
    addChildComponent(frozenBadge);
    role.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    role.setColour(juce::Label::textColourId, Theme::subduedText());
    role.setInterceptsMouseClicks(false, false);
    addChildComponent(role);

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
    const auto nowFrozen = track.isFrozen(controller.state());
    const auto nowFreezing = controller.isFreezing(track.id);
    if (isDrumTrack != track.drums || isReturnTrack != track.isReturn() || frozen != nowFrozen || freezing != nowFreezing)
    {
        isDrumTrack = track.drums;
        isReturnTrack = track.isReturn();
        frozen = nowFrozen;
        freezing = nowFreezing;
        resized();
    }
    frozenBadge.setButtonText(freezing ? juce::String(juce::CharPointer_UTF8("\xe2\x80\xa6")) : juce::String(juce::CharPointer_UTF8("\xe2\x9d\x84")));
    arm.setEnabled(! frozen);
    if (isReturnTrack)
        role.setText(text("RETURN ") + daw::returnName(track.returnSlot) + text(" · Send-Effekt"), juce::dontSendNotification);
    bool anySend = false;
    for (int slot = 0; slot < daw::maximumReturns; ++slot)
        anySend = anySend || (track.mixer->sendGain[static_cast<size_t>(slot)].load() > 0.0f && controller.state().returnTrackIndex(slot) >= 0);
    send.setToggleState(anySend, juce::dontSendNotification);
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
    frozenBadge.setVisible(frozen || freezing);
    if (frozen || freezing)
    {
        frozenBadge.setBounds(top.removeFromRight(24));
        top.removeFromRight(2);
    }
    name.setBounds(top);

    const auto roomy = area.getHeight() >= 46;
    arm.setVisible(! isDrumTrack && ! isReturnTrack);
    drums.setVisible(roomy && isDrumTrack);
    input.setVisible(roomy && ! isDrumTrack && ! isReturnTrack);
    role.setVisible(roomy && isReturnTrack);
    fx.setVisible(roomy);
    send.setVisible(roomy && ! isReturnTrack);
    monitor.setVisible(roomy && ! isDrumTrack && ! isReturnTrack);
    volume.setVisible(roomy);
    pan.setVisible(roomy);
    if (! roomy)
        return;
    area.removeFromTop(4);
    auto second = area.removeFromTop(20);
    if (! isReturnTrack)
    {
        monitor.setBounds(second.removeFromRight(30));
        second.removeFromRight(2);
    }
    fx.setBounds(second.removeFromRight(36));
    second.removeFromRight(2);
    if (! isReturnTrack)
    {
        send.setBounds(second.removeFromRight(44));
        second.removeFromRight(4);
    }
    input.setBounds(second);
    role.setBounds(second);
    if (isDrumTrack)
        drums.setBounds(second);
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
    if (! isReturnTrack)
    {
        menu.addItem(8, text("Sends …"));
        menu.addSeparator();
        const auto* track = controller.state().findTrack(id);
        const auto hasRender = track != nullptr && track->frozen != nullptr;
        menu.addItem(6, text("Spur einfrieren"), ! frozen && ! freezing);
        menu.addItem(9, text("Spur auftauen"), hasRender && ! freezing);
        menu.addItem(7, text("Einfrieren und in Audio umwandeln"), ! freezing);
    }
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
            case 6: self.controller.freezeTrack(trackId); break;
            case 7: self.controller.flattenTrack(trackId); break;
            case 8: self.showSends(); break;
            case 9: self.controller.unfreezeTrack(trackId); break;
            default: break;
        }
    });
}

namespace
{
/** Send levels of one track, one slider per existing return track. */
class SendPanel final : public juce::Component
{
public:
    SendPanel(daw::ArrangementController& c, const juce::Uuid& track) : controller(c), trackId(track)
    {
        const auto& state = controller.state();
        const auto* owner = state.findTrack(trackId);
        for (int slot = 0; slot < daw::maximumReturns && owner != nullptr; ++slot)
        {
            const auto index = state.returnTrackIndex(slot);
            if (index < 0)
                continue;
            auto& row = rows.emplace_back(std::make_unique<Row>());
            row->slot = slot;
            row->label.setText(state.tracks[static_cast<size_t>(index)].name, juce::dontSendNotification);
            row->label.setColour(juce::Label::textColourId, Theme::text());
            row->slider.setRange(-60.0, 6.0, 0.1);
            row->slider.setSkewFactorFromMidPoint(-12.0);
            row->slider.setDoubleClickReturnValue(true, -60.0);
            row->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
            row->slider.textFromValueFunction = [](double v) { return v <= -59.9 ? juce::String("aus") : juce::String(v, 1) + " dB"; };
            row->slider.valueFromTextFunction = [](const juce::String& t) { return t.trim().startsWithIgnoreCase("aus") ? -60.0 : t.getDoubleValue(); };
            const auto gain = owner->mixer->sendGain[static_cast<size_t>(slot)].load();
            row->slider.setValue(gain > 0.0f ? juce::Decibels::gainToDecibels(gain, -60.0f) : -60.0, juce::dontSendNotification);
            row->slider.onValueChange = [this, slot, slider = &row->slider]
            {
                const auto db = slider->getValue();
                controller.setSend(trackId, slot, db <= -59.9 ? 0.0f : juce::Decibels::decibelsToGain(static_cast<float>(db)));
            };
            addAndMakeVisible(row->label);
            addAndMakeVisible(row->slider);
        }
        hint.setText(rows.empty() ? text("Noch keine Return-Spur: \"+ Spur\" → \"Return-Spur (Send-Effekt)\".")
                                  : text("Post-Fader: der Send folgt Lautstärke, Panorama und Mute der Spur."),
                     juce::dontSendNotification);
        hint.setColour(juce::Label::textColourId, Theme::subduedText());
        hint.setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(hint);
        setSize(360, 34 + static_cast<int>(std::max<size_t>(rows.size(), 1)) * 30);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        hint.setBounds(area.removeFromBottom(20));
        for (auto& row : rows)
        {
            auto line = area.removeFromTop(28);
            row->label.setBounds(line.removeFromLeft(110));
            row->slider.setBounds(line);
        }
    }

private:
    struct Row
    {
        int slot = 0;
        juce::Label label;
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    };
    daw::ArrangementController& controller;
    juce::Uuid trackId;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Label hint;
};
} // namespace

void TrackHeaderComponent::showSends()
{
    juce::CallOutBox::launchAsynchronously(std::make_unique<SendPanel>(controller, id), send.getScreenBounds(), nullptr);
}
} // namespace nam::ui
