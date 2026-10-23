#include "Shortcuts.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using Theme = AmberTheme;
using daw::text;

namespace
{
constexpr auto ctrl = juce::ModifierKeys::commandModifier;
constexpr auto shift = juce::ModifierKeys::shiftModifier;

juce::KeyPress key(int code, int modifiers = 0) { return juce::KeyPress(code, juce::ModifierKeys(modifiers), 0); }
} // namespace

const std::vector<CommandDefinition>& commandDefinitions()
{
    using namespace commands;
    static const std::vector<CommandDefinition> definitions {
        // Transport
        { playStop, "Wiedergabe / Stopp", "Startet oder stoppt die Wiedergabe", "Transport", { key(juce::KeyPress::spaceKey) } },
        { record, "Aufnahme", "Startet oder beendet die Aufnahme auf den scharfen Spuren", "Transport", { key(juce::KeyPress::F9Key) } },
        { toStart, "Zum Anfang", "Setzt die Abspielposition an den Anfang", "Transport", { key(juce::KeyPress::homeKey) } },
        { loop, "Loop an/aus", "Schaltet den Loop-Bereich an oder aus", "Transport", { key('L', ctrl) } },
        { metronome, "Metronom an/aus", "Schaltet den Klick an oder aus", "Transport", {} },
        { countIn, "Count-In an/aus", "Schaltet den Vorzähler vor der Aufnahme an oder aus", "Transport", {} },
        // Bearbeiten
        { undo, "Rückgängig", "Macht den letzten Schritt rückgängig", "Bearbeiten", { key('Z', ctrl) } },
        { redo, "Wiederholen", "Stellt den rückgängig gemachten Schritt wieder her", "Bearbeiten", { key('Y', ctrl), key('Z', ctrl | shift) } },
        { split, "Clip teilen", "Teilt die ausgewählten Clips an der Abspielposition", "Bearbeiten", { key('E', ctrl) } },
        { duplicate, "Duplizieren", "Setzt eine Kopie der Auswahl direkt dahinter", "Bearbeiten", { key('D', ctrl) } },
        { copy, "Kopieren", "Kopiert die ausgewählten Clips", "Bearbeiten", { key('C', ctrl) } },
        { cut, "Ausschneiden", "Schneidet die ausgewählten Clips aus", "Bearbeiten", { key('X', ctrl) } },
        { paste, "Einfügen", "Fügt die kopierten Clips an der Abspielposition ein", "Bearbeiten", { key('V', ctrl) } },
        { selectAll, "Alles auswählen", "Wählt alle Clips aus", "Bearbeiten", { key('A', ctrl) } },
        { deleteSelection, "Löschen", "Löscht die ausgewählten Clips", "Bearbeiten", { key(juce::KeyPress::deleteKey), key(juce::KeyPress::backspaceKey) } },
        { cancel, "Abbrechen", "Bricht das laufende Ziehen ab", "Bearbeiten", { key(juce::KeyPress::escapeKey) } },
        { nextTake, "Nächster Take", "Spielt in den ausgewählten Clips den nächsten Take einer Loop-Aufnahme", "Bearbeiten", { key(']') } },
        { previousTake, "Vorheriger Take", "Spielt in den ausgewählten Clips den vorherigen Take einer Loop-Aufnahme", "Bearbeiten", { key('[') } },
        // Werkzeuge
        { pointerTool, "Werkzeug Zeiger", "Verschieben, kürzen, Fades", "Werkzeuge", { key('V') } },
        { splitTool, "Werkzeug Schere", "Klick teilt einen Clip", "Werkzeuge", { key('C') } },
        { stretchTool, "Werkzeug Stretch", "Kanten ziehen streckt per Rubber Band", "Werkzeuge", { key('T') } },
        // Ansicht
        { zoomIn, "Hineinzoomen", "Zoomt in die Zeitachse", "Ansicht", { key('+', ctrl), key('=', ctrl), key(juce::KeyPress::numberPadAdd, ctrl) } },
        { zoomOut, "Herauszoomen", "Zoomt aus der Zeitachse", "Ansicht", { key('-', ctrl), key(juce::KeyPress::numberPadSubtract, ctrl) } },
        { zoomFit, "Alles zeigen", "Zeigt das ganze Arrangement", "Ansicht", { key('Z') } },
        { follow, "Mitlaufen an/aus", "Die Ansicht folgt der Abspielposition", "Ansicht", { key('F') } },
        { toggleView, "Rig / Arrangement wechseln", "Wechselt zwischen Live-Rig und Mehrspur-Arrangement", "Ansicht", { key(juce::KeyPress::tabKey) }, false },
        { showShortcuts, "Tastenkürzel …", "Zeigt diese Übersicht", "Ansicht", { key(juce::KeyPress::F1Key) }, false },
        // Live-Rig
        { wahMouse, "Wah-Pedal per Maus an/aus", "Die vertikale Mausposition steuert das Wah-Pedal (oben Toe, unten Heel)", "Live-Rig", { key('W') }, false },
        // Spuren
        { newTrack, "Neue Audiospur", "Neue Spur mit dem NAM-Rig als Quelle", "Spuren", { key('T', ctrl) } },
        { newDrumTrack, "Neue Drum-Spur", "Neue Spur mit Drumcomputer", "Spuren", { key('T', ctrl | shift) } },
        { newReturnTrack, "Neue Return-Spur", "Neue Return-Spur für Send-Effekte (A–D)", "Spuren", { key('R', ctrl | shift) } },
        { freezeTrack, "Spur einfrieren", "Rechnet Effekte und Instrument der ausgewählten Spur vor und spart Rechenleistung", "Spuren", { key('F', ctrl | shift) } },
        { unfreezeTrack, "Spur auftauen", "Spielt die ausgewählte Spur wieder live über ihre Effekte", "Spuren", { key('U', ctrl | shift) } },
        { flattenTrack, "In Audio umwandeln", "Friert die ausgewählte Spur ein und ersetzt ihre Clips durch das Ergebnis", "Spuren", {} },
        { trackFx, "Spur-Effekte", "Öffnet die Effektkette der ausgewählten Spur", "Spuren", { key('F', ctrl | juce::ModifierKeys::altModifier) } },
        { trackSends, "Sends", "Zeigt die Send-Regler der ausgewählten Spur", "Spuren", {} },
        // Projekt
        { save, "Speichern", "Speichert das Projekt", "Projekt", { key('S', ctrl) } },
        { saveAs, "Speichern unter …", "Speichert das Projekt unter neuem Namen", "Projekt", { key('S', ctrl | shift) } },
        { open, "Projekt öffnen …", "Öffnet ein Projekt", "Projekt", { key('O', ctrl) } },
        { import, "Audio/Video importieren …", "Importiert Audiodateien oder den Ton aus Videos", "Projekt", { key('I', ctrl) } },
        { exportAudio, "Exportieren …", "Mix, Loop-Bereich oder Stems exportieren", "Projekt", { key('E', ctrl | shift) } },
    };
    return definitions;
}

const CommandDefinition* findCommand(juce::CommandID id)
{
    for (const auto& definition : commandDefinitions())
        if (definition.id == id)
            return &definition;
    return nullptr;
}

void describeCommand(juce::CommandID id, juce::ApplicationCommandInfo& info, bool arrangementVisible)
{
    const auto* definition = findCommand(id);
    if (definition == nullptr)
        return;
    info.setInfo(text(definition->name), text(definition->description), text(definition->category), 0);
    for (const auto& press : definition->defaultKeys)
        info.addDefaultKeypress(press.getKeyCode(), press.getModifiers());
    info.setActive(arrangementVisible || ! definition->needsArrangement);
}

void installGermanShortcutTexts()
{
    if (juce::LocalisedStrings::getCurrentMappings() != nullptr)
        return;
    // JUCE's key-mapping editor texts (TRANS) in German.
    const char* mapping = R"(language: German
"Adds a new key-mapping" = "Weitere Taste hinzufügen"
"Click to change this key-mapping" = "Klicken, um diese Taste zu ändern"
"Change this key-mapping" = "Diese Taste ändern"
"Remove this key-mapping" = "Diese Taste entfernen"
"New key-mapping" = "Neue Taste"
"Please press a key combination now..." = "Bitte jetzt die gewünschte Tastenkombination drücken …"
"Key" = "Taste"
"Currently assigned to \"CMDN\"" = "Bisher belegt mit \"CMDN\""
"Change key-mapping" = "Tastenkürzel ändern"
"This key is already assigned to the command \"CMDN\"" = "Diese Taste ist schon mit \"CMDN\" belegt."
"Do you want to re-assign it to this new command instead?" = "Soll sie stattdessen diesem Befehl zugewiesen werden?"
"Re-assign" = "Neu zuweisen"
"reset to defaults" = "Standard wiederherstellen"
"Reset to defaults" = "Standard wiederherstellen"
"Are you sure you want to reset all the key-mappings to their default state?" = "Alle Tastenkürzel auf die Standardbelegung zurücksetzen?"
"Reset" = "Zurücksetzen"
"Cancel" = "Abbrechen"
)";
    juce::LocalisedStrings::setCurrentMappings(new juce::LocalisedStrings(juce::String(juce::CharPointer_UTF8(mapping)), false));
}

juce::String describeKey(const juce::KeyPress& press)
{
    juce::StringArray parts;
    const auto mods = press.getModifiers();
    if (mods.isCtrlDown() || mods.isCommandDown())
        parts.add("Strg");
    if (mods.isAltDown())
        parts.add("Alt");
    if (mods.isShiftDown())
        parts.add("Umschalt");

    const auto code = press.getKeyCode();
    juce::String name;
    if (code == juce::KeyPress::spaceKey) name = "Leertaste";
    else if (code == juce::KeyPress::deleteKey) name = "Entf";
    else if (code == juce::KeyPress::backspaceKey) name = text("Rücktaste");
    else if (code == juce::KeyPress::homeKey) name = "Pos1";
    else if (code == juce::KeyPress::endKey) name = "Ende";
    else if (code == juce::KeyPress::escapeKey) name = "Esc";
    else if (code == juce::KeyPress::returnKey) name = "Enter";
    else if (code == juce::KeyPress::tabKey) name = "Tab";
    else if (code == juce::KeyPress::insertKey) name = "Einfg";
    else if (code == juce::KeyPress::pageUpKey) name = text("Bild ↑");
    else if (code == juce::KeyPress::pageDownKey) name = text("Bild ↓");
    else if (code == juce::KeyPress::leftKey) name = text("Pfeil ←");
    else if (code == juce::KeyPress::rightKey) name = text("Pfeil →");
    else if (code == juce::KeyPress::upKey) name = text("Pfeil ↑");
    else if (code == juce::KeyPress::downKey) name = text("Pfeil ↓");
    else if (code == juce::KeyPress::numberPadAdd) name = "Ziffernblock +";
    else if (code == juce::KeyPress::numberPadSubtract) name = "Ziffernblock -";
    else
    {
        // Function keys, letters and digits: JUCE's description without its modifier prefix.
        name = juce::KeyPress(code).getTextDescription();
        if (name.length() == 1)
            name = name.toUpperCase();
    }
    parts.add(name);
    return parts.joinIntoString(" + ");
}

//==============================================================================
class ShortcutEditor::Editor final : public juce::KeyMappingEditorComponent
{
public:
    explicit Editor(juce::KeyPressMappingSet& mappings) : juce::KeyMappingEditorComponent(mappings, true)
    {
        setColours(Theme::surface(), Theme::text());
    }

    juce::String getDescriptionForKeyPress(const juce::KeyPress& press) override { return describeKey(press); }
};

ShortcutEditor::ShortcutEditor(juce::KeyPressMappingSet& mappings) : editor(std::make_unique<Editor>(mappings))
{
    title.setText(text("Tastenkürzel"), juce::dontSendNotification);
    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, Theme::text());
    hint.setText(text("Klick auf eine Taste: ändern oder entfernen · \"+\": weitere Taste · Änderungen gelten sofort und werden gespeichert."),
                 juce::dontSendNotification);
    mouseTitle.setText("Maus (fest)", juce::dontSendNotification);
    mouseTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    for (auto* label : { &hint, &mouseTitle })
        label->setColour(juce::Label::textColourId, Theme::subduedText());

    gestures.setMultiLine(true);
    gestures.setReadOnly(true);
    gestures.setScrollbarsShown(true);
    gestures.setCaretVisible(false);
    gestures.setColour(juce::TextEditor::backgroundColourId, Theme::surface());
    gestures.setColour(juce::TextEditor::textColourId, Theme::text());
    gestures.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    gestures.setText(text(
        "Strg + Mausrad – Zoom um die Mausposition (auch Touchpad-Pinch)\n"
        "Mausrad / Umschalt + Mausrad – Spuren scrollen / horizontal scrollen\n"
        "Alt + Mausrad – Spurhöhe ändern\n"
        "Strg + Clip ziehen – Kopie ziehen\n"
        "Strg + Clipkante ziehen – Time-Stretch (Rubber Band)\n"
        "Alt beim Ziehen – ohne Raster\n"
        "Umschalt + Klick / Rahmen ziehen – Mehrfachauswahl\n"
        "Doppelklick auf Clip – Clip-Eigenschaften (Name, Gain, Stretch, Transponieren)\n"
        "Rechtsklick auf Spurkopf – Spur-Menü (Einfrieren, Sends, Effekte, Export …)\n"
        "Umschalt beim Malen im Drumcomputer – Akzent setzen\n"
        "Maus hoch/runter (Wah-Pedal mit MAUS an) – oben Toe (hell), unten Heel (dunkel)"), juce::dontSendNotification);

    for (auto* component : std::initializer_list<juce::Component*> { &title, &hint, editor.get(), &mouseTitle, &gestures })
        addAndMakeVisible(*component);
    setSize(640, 720);
}

void ShortcutEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background());
}

void ShortcutEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    title.setBounds(area.removeFromTop(26));
    hint.setBounds(area.removeFromTop(20));
    area.removeFromTop(6);
    auto bottom = area.removeFromBottom(170);
    editor->setBounds(area);
    bottom.removeFromTop(8);
    mouseTitle.setBounds(bottom.removeFromTop(20));
    gestures.setBounds(bottom);
}
} // namespace nam::ui
