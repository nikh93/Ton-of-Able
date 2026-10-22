#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace nam::ui
{
/** All keyboard commands of the app. The ids are stored with user key
    mappings, so existing values must never change. */
namespace commands
{
enum : juce::CommandID
{
    playStop = 0x2001, record, toStart, loop, metronome, countIn,
    undo = 0x2101, redo, split, duplicate, copy, cut, paste, selectAll, deleteSelection, cancel,
    pointerTool = 0x2201, splitTool, stretchTool,
    zoomIn = 0x2301, zoomOut, zoomFit, follow, toggleView, showShortcuts,
    wahMouse = 0x2601,
    newTrack = 0x2401, newDrumTrack, newReturnTrack, freezeTrack, unfreezeTrack, flattenTrack, trackFx, trackSends,
    save = 0x2501, saveAs, open, import, exportAudio
};
} // namespace commands

struct CommandDefinition
{
    juce::CommandID id;
    const char* name;        // UTF-8
    const char* description; // UTF-8
    const char* category;    // UTF-8
    std::vector<juce::KeyPress> defaultKeys;
    bool needsArrangement = true; // only active while the arrangement is shown
};

/** The command list with names, categories and default keys. */
const std::vector<CommandDefinition>& commandDefinitions();
const CommandDefinition* findCommand(juce::CommandID);
/** Fills an ApplicationCommandInfo from the definition. */
void describeCommand(juce::CommandID, juce::ApplicationCommandInfo&, bool arrangementVisible);

/** German texts for JUCE's key-mapping editor (installed once at start-up). */
void installGermanShortcutTexts();
/** "Strg + Umschalt + Z", "Leertaste", "Entf" ... */
juce::String describeKey(const juce::KeyPress&);

/** Shortcut overview: every command with its keys, editable (click a key to
    change it, + to add one, reset to defaults), and the fixed mouse gestures. */
class ShortcutEditor final : public juce::Component
{
public:
    explicit ShortcutEditor(juce::KeyPressMappingSet&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Editor;
    juce::Label title, hint, mouseTitle;
    std::unique_ptr<Editor> editor;
    juce::TextEditor gestures;
};
} // namespace nam::ui
