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
    name.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    name.setMinimumHorizontalScale(0.8f);
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
    arm.setIconColour(Theme::record().withAlpha(0.8f));
    arm.setColour(juce::TextButton::buttonOnColourId, Theme::record());
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
    mute.onClick = [this]
    {
        if (! mixer)
            return;
        mixer->muted = ! mixer->muted.load();
        mute.setToggleState(mixer->muted.load(), juce::dontSendNotification); // show it right away
        controller.markDirty();
    };
    solo.setTooltip("Solo");
    solo.onClick = [this]
    {
        if (! mixer)
            return;
        mixer->soloed = ! mixer->soloed.load();
        solo.setToggleState(mixer->soloed.load(), juce::dontSendNotification);
        controller.markDirty();
    };
    // Active mute is yellow, active solo blue (as in Ableton), both with dark text.
    mute.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe8b33c));
    solo.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff5aa9e6));
    for (auto* button : { &mute, &solo })
        button->setColour(juce::TextButton::textColourOnId, Theme::onText());
    monitor.setIconColour(Theme::subduedText());
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
    instrumentButton.setTooltip(text("Instrument dieser MIDI-Spur: eingebauter Synth oder ein VST3-Instrument (z. B. Vital)"));
    instrumentButton.onClick = [this] { if (onInstrumentMenu) onInstrumentMenu(id, instrumentButton); };
    addChildComponent(instrumentButton);
    for (juce::Button* button : std::initializer_list<juce::Button*> { &arm, &mute, &solo, &monitor, &fx })
        addAndMakeVisible(*button);
    send.setTooltip(text("Sends: wie viel dieser Spur in die Return-Spuren (Hall, Delay …) geht"));
    send.onClick = [this] { showSends(); };
    addAndMakeVisible(send);
    frozenBadge.setTooltip(text("Eingefroren: spielt eine vorberechnete Datei statt Effekten und Instrument. Klick zum Auftauen."));
    frozenBadge.setIconColour(juce::Colour(0xff9fd8ff));
    frozenBadge.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff23323d));
    frozenBadge.onClick = [this] { if (frozen) controller.unfreezeTrack(id); };
    addChildComponent(frozenBadge);
    reampBadge.setTooltip(text("Re-Amp: Diese Spur spielt bei der Wiedergabe statt der Gitarre ins Live-Rig. Eine scharfgeschaltete Rig-Spur nimmt das Ergebnis auf. Klick schaltet aus."));
    reampBadge.setColour(juce::TextButton::buttonColourId, Theme::amber());
    reampBadge.setColour(juce::TextButton::textColourOffId, Theme::onText());
    reampBadge.onClick = [this] { controller.setReampSource({}); };
    addChildComponent(reampBadge);
    chordBadge.setButtonText(text("AKKORDE"));
    chordBadge.setTooltip(text("Begleitung aus der Akkordleiste: Die Noten folgen jeder Akkordänderung. Klick: Stil ändern. "
                               "Noten von Hand bearbeiten löst die Verknüpfung."));
    chordBadge.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff6fc0a4));
    chordBadge.setColour(juce::TextButton::textColourOffId, Theme::onText());
    chordBadge.onClick = [this] { if (onEditAccompaniment) onEditAccompaniment(id); };
    addChildComponent(chordBadge);
    role.setFont(juce::FontOptions(11.5f, juce::Font::bold));
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
    const auto* source = controller.state().reampTrack();
    const auto nowReamping = source != nullptr && source->id == track.id && ! nowFrozen;
    if (isDrumTrack != track.drums || isMidiTrack != track.midi || isReturnTrack != track.isReturn() || frozen != nowFrozen
        || freezing != nowFreezing || reamping != nowReamping
        || linkedToChords != (track.accompaniment && track.accompaniment->linked) || generated != track.accompaniment.has_value())
    {
        linkedToChords = track.accompaniment && track.accompaniment->linked;
        generated = track.accompaniment.has_value();
        isMidiTrack = track.midi;
        arm.setTooltip(track.midi ? text("Scharf: die MIDI-Keyboards spielen diese Spur, die Aufnahme nimmt Noten auf")
                                  : text("Aufnahme scharf schalten"));
        reamping = nowReamping;
        isDrumTrack = track.drums;
        isReturnTrack = track.isReturn();
        frozen = nowFrozen;
        freezing = nowFreezing;
        resized();
    }
    if (track.midi)
    {
        const auto name = track.instrument->instrument != nullptr ? track.instrument->instrument->name() : text("Instrument");
        if (instrumentButton.getButtonText() != name)
            instrumentButton.setButtonText(name);
    }
    frozenBadge.setAlpha(freezing ? 0.5f + 0.5f * static_cast<float>((juce::Time::getMillisecondCounter() / 300) % 2) : 1.0f);
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
    monitor.setToggleState(track.monitorInput, juce::dontSendNotification);
    monitor.setEnabled(track.inputChannel >= 0);
    fx.setToggleState(track.fx->enabled.load(), juce::dontSendNotification);
    fx.setButtonText(track.fx->enabled.load() && track.fx->printWhileRecording.load() && track.inputChannel >= 0 ? "FX P" : "FX");
    name.setColour(juce::Label::textColourId, selected ? Theme::amberBright() : Theme::text());
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
    g.setColour(isSelected ? juce::Colour(0xff2b241e) : Theme::panel());
    g.fillRect(bounds);
    if (isSelected)
    {
        g.setColour(Theme::amber().withAlpha(0.06f));
        g.fillRect(bounds);
    }
    // Track colour as a slim bar on the left, wider when selected.
    g.setColour(colour);
    g.fillRect(bounds.removeFromLeft(isSelected ? 5.0f : 3.0f));
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

    // Level meter: a slim rounded bar along the right edge.
    auto meter = getLocalBounds().removeFromRight(9).reduced(3, 6).toFloat();
    g.setColour(Theme::well());
    g.fillRoundedRectangle(meter, 1.5f);
    const auto height = meter.getHeight() * meterLevel;
    if (height > 0.5f)
    {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff6fbf73), 0.0f, meter.getBottom(), Theme::amber(), 0.0f, meter.getY(), false));
        if (meterLevel > 0.97f)
            g.setColour(Theme::record());
        g.fillRoundedRectangle(meter.withTop(meter.getBottom() - height), 1.5f);
    }
}

void TrackHeaderComponent::resized()
{
    auto area = getLocalBounds().withTrimmedLeft(12).withTrimmedRight(12).reduced(0, 5);
    auto top = area.removeFromTop(22);
    const auto square = [&top](juce::Component& component, int width = 24)
    {
        component.setBounds(top.removeFromRight(width));
        top.removeFromRight(3);
    };
    square(solo);
    square(mute);
    if (! isDrumTrack && ! isReturnTrack)
        square(arm, 26);
    frozenBadge.setVisible(frozen || freezing);
    if (frozen || freezing)
        square(frozenBadge, 26);
    reampBadge.setVisible(reamping);
    if (reamping)
        square(reampBadge, 56);
    chordBadge.setVisible(linkedToChords);
    if (linkedToChords)
        square(chordBadge, 62);
    top.removeFromRight(3);
    name.setBounds(top.withTrimmedLeft(-4));

    const auto roomy = area.getHeight() >= 46;
    arm.setVisible(! isDrumTrack && ! isReturnTrack);
    drums.setVisible(roomy && isDrumTrack);
    input.setVisible(roomy && ! isDrumTrack && ! isReturnTrack && ! isMidiTrack);
    instrumentButton.setVisible(roomy && isMidiTrack);
    role.setVisible(roomy && isReturnTrack);
    fx.setVisible(roomy);
    send.setVisible(roomy && ! isReturnTrack);
    monitor.setVisible(roomy && ! isDrumTrack && ! isReturnTrack && ! isMidiTrack);
    volume.setVisible(roomy);
    pan.setVisible(roomy);
    if (! roomy)
        return;
    area.removeFromTop(6);
    auto second = area.removeFromTop(22);
    // FX and SEND form one group, the source (input, drum editor or return role) fills the rest.
    if (! isReturnTrack)
    {
        send.setBounds(second.removeFromRight(46));
        send.setConnectedEdges(juce::Button::ConnectedOnLeft);
        fx.setBounds(second.removeFromRight(38));
        fx.setConnectedEdges(juce::Button::ConnectedOnRight);
    }
    else
    {
        fx.setBounds(second.removeFromRight(46));
        fx.setConnectedEdges(0);
    }
    second.removeFromRight(6);
    if (isMidiTrack)
        instrumentButton.setBounds(second);
    else if (! isDrumTrack && ! isReturnTrack)
    {
        monitor.setBounds(second.removeFromRight(28));
        second.removeFromRight(3);
        input.setBounds(second);
    }
    role.setBounds(second);
    if (isDrumTrack)
        drums.setBounds(second);
    area.removeFromTop(4);
    auto third = area.removeFromTop(24);
    pan.setBounds(third.removeFromRight(26));
    third.removeFromRight(6);
    volume.setBounds(third.withTrimmedLeft(-6));
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
        if (! isDrumTrack && ! isMidiTrack)
            menu.addItem(10, text("Re-Amp: ins Live-Rig spielen"), ! frozen, reamping);
        if (generated)
        {
            menu.addSeparator();
            menu.addItem(11, text("Begleitung: Stil ändern …"));
            if (linkedToChords)
                menu.addItem(13, text("Von den Akkorden lösen (Noten behalten)"));
            else
                menu.addItem(12, text("Wieder aus den Akkorden erzeugen (eigene Änderungen gehen verloren)"));
        }
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
            case 10: self.controller.setReampSource(self.reamping ? juce::Uuid() : trackId); break;
            case 11: if (self.onEditAccompaniment) self.onEditAccompaniment(trackId); break;
            case 12: self.controller.relinkAccompaniment(trackId); break;
            case 13: self.controller.unlinkAccompaniment(trackId); break;
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
