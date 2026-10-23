#include "GuitarSynthPanel.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
namespace
{
using Theme = AmberTheme;
using daw::text;
constexpr int noneId = 1;
constexpr int firstPluginId = 100;

class PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow(const juce::String& name, juce::AudioProcessorEditor* editor)
        : DocumentWindow(name, Theme::surface(), closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true); // deleting the editor detaches it from the plugin
        setResizable(editor->isResizable(), false);
        centreWithSize(getWidth(), getHeight());
    }
    void closeButtonPressed() override { setVisible(false); }
};
} // namespace

GuitarSynthPanel::GuitarSynthPanel(GuitarSynth& s, Host h) : synth(s), host(std::move(h))
{
    title.setText(text("Gitarre → MIDI → Synth"), juce::dontSendNotification);
    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, Theme::text());
    addAndMakeVisible(title);

    power.setButtonText("AN");
    power.setTooltip(text("Synth-Ebene an/aus"));
    power.onClick = [this] { synth.setEnabled(power.getToggleState()); if (host.changed) host.changed(); };
    addAndMakeVisible(power);

    for (auto* label : { &trackerLabel, &synthLabel, &mixLabel })
    {
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, Theme::amber());
        addAndMakeVisible(*label);
    }
    trackerLabel.setText(text("1  ERKENNUNG  ·  Audio → MIDI (z. B. TONIQ, MIDI Guitar, Transkr, MiGiC)"), juce::dontSendNotification);
    synthLabel.setText(text("2  SYNTH  ·  Instrument (z. B. Vital)"), juce::dontSendNotification);
    mixLabel.setText(text("3  MIX"), juce::dontSendNotification);

    trackerBox.setTooltip(text("Plugin, das die Gitarre hört und Noten als MIDI ausgibt. Es bekommt das trockene Gitarrensignal."));
    synthBox.setTooltip(text("Instrument-Plugin, das die erkannten Noten spielt."));
    trackerBox.onChange = [this] { choose(GuitarSynth::Slot::tracker, trackerBox.getSelectedId()); };
    synthBox.onChange = [this] { choose(GuitarSynth::Slot::synth, synthBox.getSelectedId()); };
    trackerEditor.onClick = [this] { openEditor(GuitarSynth::Slot::tracker); };
    synthEditor.onClick = [this] { openEditor(GuitarSynth::Slot::synth); };
    trackerRemove.onClick = [this] { choose(GuitarSynth::Slot::tracker, noneId); };
    synthRemove.onClick = [this] { choose(GuitarSynth::Slot::synth, noneId); };
    trackerEditor.setTooltip(text("Oberfläche des Erkennungs-Plugins öffnen (Empfindlichkeit, Modus …)"));
    synthEditor.setTooltip(text("Oberfläche des Synths öffnen (Sound auswählen)"));
    for (juce::Component* component : std::initializer_list<juce::Component*> { &trackerBox, &synthBox, &trackerEditor, &synthEditor, &trackerRemove, &synthRemove })
        addAndMakeVisible(*component);

    scan.setTooltip(text("Sucht VST3-Plugins in den Standardordnern (jedes Plugin in einem eigenen Prozess)"));
    scan.onClick = [this]
    {
        if (! host.rescan)
            return;
        scan.setEnabled(false);
        scanInfo.setText(text("Suche läuft …"), juce::dontSendNotification);
        host.rescan([safe = juce::Component::SafePointer<GuitarSynthPanel>(this)]
        {
            if (safe == nullptr)
                return;
            safe->scan.setEnabled(true);
            safe->refresh();
        });
    };
    addAndMakeVisible(scan);
    scanInfo.setColour(juce::Label::textColourId, Theme::subduedText());
    scanInfo.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(scanInfo);

    const auto setupLevel = [this](juce::Slider& slider, juce::Label& label, std::function<void(float)> apply)
    {
        slider.setRange(-60.0, 12.0, 0.1);
        slider.setSkewFactorFromMidPoint(-12.0);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
        slider.textFromValueFunction = [](double v) { return v <= -59.9 ? juce::String("aus") : juce::String(v, 1) + " dB"; };
        slider.valueFromTextFunction = [](const juce::String& t) { return t.trim().startsWithIgnoreCase("aus") ? -60.0 : t.getDoubleValue(); };
        slider.setDoubleClickReturnValue(true, 0.0);
        slider.onValueChange = [&slider, apply] { apply(static_cast<float>(slider.getValue())); };
        slider.updateText();
        label.setColour(juce::Label::textColourId, Theme::text());
        addAndMakeVisible(slider);
        addAndMakeVisible(label);
    };
    setupLevel(synthLevel, synthLevelLabel, [this](float db) { synth.setSynthLevelDb(db); });
    setupLevel(guitarLevel, guitarLevelLabel, [this](float db) { synth.setGuitarLevelDb(db); });
    synthLevel.setTooltip(text("Lautstärke des Synths"));
    guitarLevel.setTooltip(text("Lautstärke der Gitarre neben dem Synth – ganz links = nur Synth"));

    for (int semitones : { -24, -12, -7, -5, 0, 5, 7, 12, 24 })
        transpose.addItem((semitones > 0 ? "+" : "") + juce::String(semitones) + text(" Halbtöne")
                              + (std::abs(semitones) == 12 ? text(" (Oktave)") : std::abs(semitones) == 24 ? text(" (2 Oktaven)") : juce::String()),
                          semitones + 100);
    transpose.setTooltip(text("Verschiebt die erkannten Noten, bevor sie den Synth erreichen"));
    transpose.onChange = [this] { synth.setTranspose(transpose.getSelectedId() - 100); };
    transposeLabel.setColour(juce::Label::textColourId, Theme::text());
    addAndMakeVisible(transpose);
    addAndMakeVisible(transposeLabel);

    keyboards.setTooltip(text("Angeschlossene MIDI-Keyboards und -Controller spielen den Synth zusätzlich"));
    keyboards.onClick = [this] { synth.setKeyboardsEnabled(keyboards.getToggleState()); };
    addAndMakeVisible(keyboards);

    hint.setText(text("Tipp: Für schnelle Erkennung im Audio-Setup eine kleine Puffergröße wählen (64–128 Samples). "
                      "Die Synth-Spur landet mit in Aufnahmen vom Rig; der Synth läuft vor dem VST3 hinter dem Amp."),
                 juce::dontSendNotification);
    hint.setColour(juce::Label::textColourId, Theme::subduedText());
    hint.setFont(juce::FontOptions(12.0f));
    hint.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(hint);

    setSize(640, 470);
    refresh();
    startTimerHz(30);
}

GuitarSynthPanel::~GuitarSynthPanel()
{
    stopTimer();
    closeEditors();
}

void GuitarSynthPanel::closeEditors()
{
    for (auto& window : editors)
        window.reset();
}

void GuitarSynthPanel::fillLists()
{
    trackers.clear();
    instruments.clear();
    if (host.plugins)
        for (const auto& description : host.plugins())
        {
            if (description.isInstrument)
                instruments.add(description);
            if (description.numInputChannels > 0)
                trackers.add(description);
        }
    const auto fill = [](juce::ComboBox& box, const juce::Array<juce::PluginDescription>& list, const juce::String& current, const juce::String& empty)
    {
        box.clear(juce::dontSendNotification);
        box.addItem(empty, noneId);
        bool found = current.isEmpty();
        for (int index = 0; index < list.size(); ++index)
        {
            box.addItem(list[index].name + (list[index].manufacturerName.isNotEmpty() ? "  (" + list[index].manufacturerName + ")" : juce::String()),
                        firstPluginId + index);
            if (list[index].name == current)
            {
                box.setSelectedId(firstPluginId + index, juce::dontSendNotification);
                found = true;
            }
        }
        if (current.isEmpty())
            box.setSelectedId(noneId, juce::dontSendNotification);
        else if (! found)
            box.setText(current, juce::dontSendNotification);
    };
    fill(trackerBox, trackers, synth.name(GuitarSynth::Slot::tracker), text("– keine Erkennung (nur MIDI-Keyboard) –"));
    fill(synthBox, instruments, synth.name(GuitarSynth::Slot::synth), text("– kein Synth –"));
    scanInfo.setText(juce::String(instruments.size()) + text(" Instrumente, ") + juce::String(trackers.size())
                         + text(" Plugins mit Audio-Eingang gefunden"),
                     juce::dontSendNotification);
}

void GuitarSynthPanel::refresh()
{
    fillLists();
    power.setToggleState(synth.isEnabled(), juce::dontSendNotification);
    synthLevel.setValue(synth.synthLevel(), juce::dontSendNotification);
    guitarLevel.setValue(synth.guitarLevel(), juce::dontSendNotification);
    transpose.setSelectedId(synth.transposeSemitones() + 100, juce::dontSendNotification);
    if (transpose.getSelectedId() == 0)
        transpose.setText(juce::String(synth.transposeSemitones()) + text(" Halbtöne"), juce::dontSendNotification);
    keyboards.setToggleState(synth.keyboardsEnabled(), juce::dontSendNotification);
    trackerEditor.setEnabled(synth.hasPlugin(GuitarSynth::Slot::tracker));
    trackerRemove.setEnabled(synth.hasPlugin(GuitarSynth::Slot::tracker));
    synthEditor.setEnabled(synth.hasPlugin(GuitarSynth::Slot::synth));
    synthRemove.setEnabled(synth.hasPlugin(GuitarSynth::Slot::synth));
}

void GuitarSynthPanel::choose(GuitarSynth::Slot slot, int comboId)
{
    const auto index = slot == GuitarSynth::Slot::tracker ? 0 : 1;
    editors[index].reset(); // the editor belongs to the old plugin
    const auto& list = slot == GuitarSynth::Slot::tracker ? trackers : instruments;
    if (comboId == noneId)
        synth.clear(slot);
    else if (juce::isPositiveAndBelow(comboId - firstPluginId, list.size()))
    {
        const auto& description = list.getReference(comboId - firstPluginId);
        if (const auto result = synth.load(slot, description); result.failed())
        {
            if (host.status)
                host.status(result.getErrorMessage());
        }
        else
        {
            // Choosing a plugin means wanting to hear it.
            synth.setEnabled(true);
            if (host.status)
                host.status(description.name + text(" geladen."));
        }
    }
    refresh();
    if (host.changed)
        host.changed();
}

void GuitarSynthPanel::openEditor(GuitarSynth::Slot slot)
{
    const auto index = slot == GuitarSynth::Slot::tracker ? 0 : 1;
    if (editors[index] != nullptr)
    {
        editors[index]->setVisible(true);
        editors[index]->toFront(true);
        return;
    }
    auto* plugin = synth.instance(slot);
    if (plugin == nullptr)
        return;
    auto* editor = plugin->createEditorIfNeeded();
    if (editor == nullptr)
    {
        if (host.status)
            host.status(synth.name(slot) + text(" hat keine eigene Oberfläche."));
        return;
    }
    editors[index] = std::make_unique<PluginEditorWindow>(synth.name(slot), editor);
    editors[index]->setVisible(true);
}

juce::String GuitarSynthPanel::noteName(int note) const
{
    return note < 0 ? text("–") : juce::MidiMessage::getMidiNoteName(note, true, true, 4);
}

void GuitarSynthPanel::timerCallback()
{
    const auto count = synth.noteCount();
    if (count != lastCount)
    {
        lastCount = count;
        flash = 1.0f;
    }
    else
        flash = std::max(0.0f, flash - 0.08f);
    repaint(noteArea);
}

void GuitarSynthPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::panel());
    // Live note display: the last recognised note, lit while notes are held.
    auto area = noteArea.toFloat();
    g.setColour(Theme::well());
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(area.reduced(0.5f), 8.0f, 1.0f);
    const auto holding = synth.heldNotes() > 0;
    auto led = area.removeFromLeft(34.0f).withSizeKeepingCentre(12.0f, 12.0f);
    const auto lit = holding ? 1.0f : flash;
    g.setColour(Theme::amberBright().withAlpha(0.15f + 0.85f * lit));
    g.fillEllipse(led);
    g.setColour(lit > 0.05f ? Theme::amberBright() : Theme::subduedText());
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::bold));
    g.drawText(noteName(synth.lastNote()), area.removeFromLeft(80.0f), juce::Justification::centredLeft);
    g.setColour(Theme::subduedText());
    g.setFont(juce::FontOptions(12.0f));
    g.drawFittedText(! synth.isEnabled() ? text("Aus – oben rechts einschalten")
                     : synth.noteCount() == 0 ? text("Noch keine Noten erkannt – spiel ein paar Töne")
                                              : juce::String(synth.noteCount()) + text(" Noten erkannt"),
                     area.reduced(8.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
}

void GuitarSynthPanel::resized()
{
    auto area = getLocalBounds().reduced(18, 14);
    auto top = area.removeFromTop(30);
    power.setBounds(top.removeFromRight(70));
    title.setBounds(top);
    area.removeFromTop(10);

    const auto pluginRow = [&area](juce::Label& label, juce::ComboBox& box, juce::Button& editor, juce::Button& remove)
    {
        label.setBounds(area.removeFromTop(20));
        auto row = area.removeFromTop(30);
        remove.setBounds(row.removeFromRight(92));
        row.removeFromRight(6);
        editor.setBounds(row.removeFromRight(92));
        row.removeFromRight(8);
        box.setBounds(row);
        area.removeFromTop(12);
    };
    pluginRow(trackerLabel, trackerBox, trackerEditor, trackerRemove);
    pluginRow(synthLabel, synthBox, synthEditor, synthRemove);
    auto scanRow = area.removeFromTop(28);
    scan.setBounds(scanRow.removeFromLeft(190));
    scanRow.removeFromLeft(10);
    scanInfo.setBounds(scanRow);
    area.removeFromTop(14);

    mixLabel.setBounds(area.removeFromTop(20));
    const auto levelRow = [&area](juce::Label& label, juce::Component& control)
    {
        auto row = area.removeFromTop(28);
        label.setBounds(row.removeFromLeft(110));
        control.setBounds(row);
        area.removeFromTop(4);
    };
    levelRow(synthLevelLabel, synthLevel);
    levelRow(guitarLevelLabel, guitarLevel);
    auto row = area.removeFromTop(28);
    transposeLabel.setBounds(row.removeFromLeft(110));
    transpose.setBounds(row.removeFromLeft(220));
    row.removeFromLeft(14);
    keyboards.setBounds(row);
    area.removeFromTop(12);
    noteArea = area.removeFromTop(44);
    area.removeFromTop(8);
    hint.setBounds(area);
}
} // namespace nam::ui
