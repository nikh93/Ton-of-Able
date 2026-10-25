#include "AccompanimentPanel.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using daw::text;
using Theme = AmberTheme;
using Layer = daw::AccompanimentStyle::Layer;

AccompanimentPanel::AccompanimentPanel(daw::ArrangementController& c, const juce::Uuid& track, std::function<juce::ValueTree()> effect)
    : controller(c), editTrack(track), spaceEffect(std::move(effect))
{
    const auto* edited = controller.state().findTrack(editTrack);
    const auto editing = edited != nullptr && edited->accompaniment.has_value();
    const auto current = editing ? edited->accompaniment->style : daw::AccompanimentStyle {};

    heading.setText(editing ? text("Begleitung: ") + edited->name : text("Begleitung aus Akkorden"), juce::dontSendNotification);
    heading.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    heading.setColour(juce::Label::textColourId, Theme::amberBright());
    const auto chords = controller.state().chords.size();
    summary.setText(editing ? text("Die Spur folgt den Akkorden. Änderungen hier erzeugen sie neu.")
                            : (chords == 0 ? text("Noch keine Akkorde in der Leiste – die Spuren füllen sich, sobald welche dastehen.")
                                           : juce::String(static_cast<int>(chords)) + text(" Akkorde. Die neuen MIDI-Spuren bleiben mit der Akkordleiste verknüpft: "
                                                                                         "Ändert man Akkorde, ändert sich die Begleitung mit. Wer Noten von Hand bearbeitet, löst die Verknüpfung.")),
                     juce::dontSendNotification);
    summary.setColour(juce::Label::textColourId, Theme::subduedText());
    summary.setJustificationType(juce::Justification::topLeft);
    summary.setMinimumHorizontalScale(1.0f);

    const auto setUpToggle = [this](juce::ToggleButton& button, const juce::String& label, const juce::String& tip, bool on)
    {
        button.setButtonText(label);
        button.setTooltip(tip);
        button.setToggleState(on, juce::dontSendNotification);
        button.setColour(juce::ToggleButton::textColourId, Theme::text());
        button.setColour(juce::ToggleButton::tickColourId, Theme::amber());
        addAndMakeVisible(button);
    };
    setUpToggle(pad, text("Pad – liegende Flächen, weich verbunden"), text("Volle Akkorde in offener Lage; gemeinsame Töne klingen weiter."), ! editing || current.layer == Layer::pad);
    setUpToggle(bass, text("Sub-Bass – Grundton bzw. Basston tief"), text("Grundton oder Slash-Bass (G/H spielt H), weicher Sub-Klang."), ! editing ? true : current.layer == Layer::bass);
    setUpToggle(shimmer, text("Shimmer – Glitzer-Arpeggio hoch oben"), text("Die Akkordtöne zwei Oktaven höher, auf und ab, jede Note klingt nach."), ! editing ? true : current.layer == Layer::shimmer);
    setUpToggle(swell, text("Swell – Pad schwillt bei jedem Akkord langsam an"), text("Synth-Klang „Swell“ statt „Atmo“ (sehr langsamer Einsatz)."), false);
    setUpToggle(space, text("Raum – alles in eine Return-Spur „Atmo-Hall“ schicken"), text("Legt eine Return-Spur an (Valhalla Supermassive, sonst Dragonfly oder der eingebaute Hall) und stellt die Sends ein."), true);
    if (editing)
        for (auto* button : { &pad, &bass, &shimmer, &swell, &space })
            button->setVisible(false);
    if (editing && current.layer != Layer::shimmer)
        rhythm.setEnabled(false);

    const auto setUpChoice = [this](juce::Label& label, const juce::String& name, juce::ComboBox& box, const juce::StringArray& items, int selected)
    {
        label.setText(name, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, Theme::subduedText());
        addAndMakeVisible(label);
        box.addItemList(items, 1);
        box.setSelectedItemIndex(selected, juce::dontSendNotification);
        addAndMakeVisible(box);
    };
    setUpChoice(rangeLabel, text("Lage"), range, { text("Tief"), text("Mittel"), text("Hoch") }, current.range);
    setUpChoice(colourLabel, text("Farben"), colour, { text("Wie notiert"), text("Dezent (add9)"), text("Reich (7, 9, 11, 13)") }, current.colour);
    setUpChoice(rhythmLabel, text("Shimmer"), rhythm, { text("Viertel"), text("Achtel"), text("Sechzehntel") }, current.rhythm);

    heading.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(heading);
    addAndMakeVisible(summary);
    ok.setButtonText(editing ? text("Übernehmen") : text("Erzeugen"));
    ok.setColour(juce::TextButton::buttonColourId, Theme::amber());
    ok.setColour(juce::TextButton::textColourOffId, Theme::onText());
    ok.onClick = [this] { apply(); };
    cancel.setButtonText(text("Abbrechen"));
    cancel.onClick = [this] { if (onDone) onDone(); };
    addAndMakeVisible(ok);
    addAndMakeVisible(cancel);
    setSize(520, editing ? 290 : 430);
}

daw::AccompanimentStyle AccompanimentPanel::styleFor(Layer layer) const
{
    daw::AccompanimentStyle style;
    style.layer = layer;
    style.range = std::max(0, range.getSelectedItemIndex());
    style.colour = std::max(0, colour.getSelectedItemIndex());
    style.rhythm = std::max(0, rhythm.getSelectedItemIndex());
    return style;
}

void AccompanimentPanel::apply()
{
    if (const auto* edited = controller.state().findTrack(editTrack); edited != nullptr && edited->accompaniment)
        controller.setAccompanimentStyle(editTrack, styleFor(edited->accompaniment->style.layer));
    else
    {
        daw::ArrangementController::AccompanimentRequest request;
        if (pad.getToggleState()) request.layers.push_back(styleFor(Layer::pad));
        if (bass.getToggleState()) request.layers.push_back(styleFor(Layer::bass));
        if (shimmer.getToggleState()) request.layers.push_back(styleFor(Layer::shimmer));
        if (request.layers.empty())
            return;
        request.swell = swell.getToggleState();
        request.space = space.getToggleState();
        if (request.space && spaceEffect)
            request.spaceEffect = spaceEffect();
        controller.createAccompaniment(request);
    }
    if (onDone)
        onDone();
}

void AccompanimentPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::surface());
}

void AccompanimentPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    heading.setBounds(area.removeFromTop(28));
    summary.setBounds(area.removeFromTop(52));
    area.removeFromTop(6);
    for (auto* button : { &pad, &bass, &shimmer, &swell, &space })
        if (button->isVisible())
            button->setBounds(area.removeFromTop(26));
    area.removeFromTop(10);
    for (auto [label, box] : { std::pair { &rangeLabel, &range }, std::pair { &colourLabel, &colour }, std::pair { &rhythmLabel, &rhythm } })
    {
        auto row = area.removeFromTop(28);
        label->setBounds(row.removeFromLeft(90));
        box->setBounds(row.removeFromLeft(220).reduced(0, 2));
        area.removeFromTop(4);
    }
    auto buttons = area.removeFromBottom(32);
    ok.setBounds(buttons.removeFromRight(120));
    buttons.removeFromRight(8);
    cancel.setBounds(buttons.removeFromRight(110));
}
} // namespace nam::ui
