#pragma once

#include <functional>
#include <map>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "presets/ChainLayout.h"

namespace nam::ui
{
/** The signal chain as an editor.

    Top row: the serial path from the guitar (IN) to the output (OUT). Bottom
    row (when used): blocks beside the chain, e.g. a synth, with the MIDI
    keyboards (KEYS) as a source. Curves show the extra connections: audio in
    amber, MIDI in blue.

    - LED: on/off. Click: open the block (settings or plugin window).
    - Drag a block to reorder it; plugins can also move between the rows.
    - "+" adds a VST3 plugin (effect, instrument, audio-to-MIDI) or a MIDI tool.
    - Right-click: connect its audio or MIDI output to any block, receive from
      any block, set the levels of its connections, move or remove it.

    The owner describes each block and reacts to the callbacks; layout edits
    come back through onLayoutChanged. */
class SignalChainStrip final : public juce::Component, public juce::TooltipClient
{
public:
    struct Block
    {
        juce::String label;       // short name, e.g. "DRIVE"
        juce::String detail;      // current setting, e.g. "Crunch" or the model name
        bool active = false;      // processing (LED lit)
        bool toggleable = true;   // has an on/off switch
        bool empty = false;       // nothing loaded yet: drawn hollow
        juce::String tooltip;
    };

    std::function<Block(const chain::Node&)> describe;
    std::function<void(const juce::String& id)> onToggle;
    std::function<void(const juce::String& id)> onOpen;
    std::function<void(const chain::Layout&)> onLayoutChanged;
    /** All scanned VST3 plugins, and a rescan that calls back when done. */
    std::function<juce::Array<juce::PluginDescription>()> plugins;
    std::function<void(std::function<void()> done)> rescanPlugins;
    /** MIDI keyboards exist only in the live rig. */
    bool offerKeyboards = true;

    void setLayout(const chain::Layout&);
    [[nodiscard]] const chain::Layout& layout() const noexcept { return current; }
    /** Re-reads the block descriptions (states changed). */
    void refresh();
    /** Height for the current content; wider than available when many blocks. */
    [[nodiscard]] int preferredHeight() const;
    [[nodiscard]] int preferredWidth(int available) const;
    /** Screen area of a block, e.g. to anchor a call-out to it. */
    [[nodiscard]] juce::Rectangle<int> screenBoundsOf(const juce::String& id) const;
    void setSelected(const juce::String& id);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    enum class ItemType { block, input, output, keys, addSerial, addSide };
    struct Item
    {
        ItemType type;
        juce::String id;
        juce::Rectangle<float> bounds;
        bool sideRow = false;
    };

    void layoutItems();
    [[nodiscard]] int itemAt(juce::Point<float>) const;
    [[nodiscard]] const Item* findItem(const juce::String& id) const;
    [[nodiscard]] juce::Rectangle<float> ledBounds(const Item&) const;
    [[nodiscard]] bool hasSideRow() const;
    [[nodiscard]] juce::String nameOf(const juce::String& id) const;
    void change(const chain::Layout&);
    void showAddMenu(bool side);
    void showBlockMenu(const juce::String& id);
    void showConnections(const juce::String& id);
    [[nodiscard]] int dropIndex(juce::Point<float>, bool& toSide) const;
    void paintLinks(juce::Graphics&);

    chain::Layout current;
    std::vector<Item> items;
    std::map<juce::String, Block> described;
    juce::String selected;
    int hovered = -1;
    bool hoveredLed = false;
    int pressed = -1;
    bool dragging = false;
    juce::Point<float> dragPosition;
};
} // namespace nam::ui
