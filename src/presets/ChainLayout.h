#pragma once

#include <vector>

#include <juce_data_structures/juce_data_structures.h>

/** The editable signal chain of a sound.

    The serial path runs from the guitar ("input") through the serial blocks
    to "output". Built-in effects are always in it, exactly once; plugins and
    MIDI tools can be in it or beside it ("side" blocks, e.g. a synth). Links
    add further connections: the audio or MIDI output of any block (or the
    "input" guitar, or "keys" = MIDI keyboards) to the input of another block
    (or "output"), each with a level. Inputs sum everything connected to them. */
namespace chain
{
inline constexpr const char* input = "input";   // guitar after the input gain
inline constexpr const char* keys = "keys";     // MIDI keyboards (live rig)
inline constexpr const char* output = "output"; // before the output gain

/** Built-in effects in their standard order. */
[[nodiscard]] const juce::StringArray& builtinKinds();
[[nodiscard]] bool isBuiltin(const juce::String& kind);

struct Node
{
    juce::String id;          // unique; built-ins use their kind
    juce::String kind;        // a built-in kind, "vst3" or "transpose"
    juce::String name;        // plugin name for display
    juce::ValueTree plugin;   // vst3: the plugin's saved state (Vst3Slot tree)
    bool bypassed = false;    // vst3 / transpose; built-ins keep their own switch in SoundState
    int semitones = 0;        // transpose

    [[nodiscard]] bool isBuiltin() const { return chain::isBuiltin(kind); }
    [[nodiscard]] bool handlesMidi() const { return kind == "vst3" || kind == "transpose"; }
    bool operator==(const Node&) const;
};

struct Link
{
    juce::String from, to;
    bool midi = false;
    float gainDb = 0.0f;      // audio links: level of this connection
    bool operator==(const Link& other) const
    {
        return from == other.from && to == other.to && midi == other.midi && gainDb == other.gainDb;
    }
};

struct Layout
{
    std::vector<Node> serial, side;
    std::vector<Link> links;
    float chainLevelDb = 0.0f;  // level of the serial path into the output (-60 = off, e.g. synth only)

    /** The classic fixed chain; the compressor as a pedal or after the amp. */
    [[nodiscard]] static Layout standard(bool compressorBeforeAmp = false);

    [[nodiscard]] juce::ValueTree toValueTree() const;
    /** Repairs what it reads: unknown kinds and dangling links are dropped,
        missing built-ins return at their standard place. */
    [[nodiscard]] static Layout fromValueTree(const juce::ValueTree&);

    [[nodiscard]] const Node* find(const juce::String& id) const;
    [[nodiscard]] Node* find(const juce::String& id);
    [[nodiscard]] int serialIndex(const juce::String& id) const;
    [[nodiscard]] bool isSerial(const juce::String& id) const { return serialIndex(id) >= 0; }
    /** Both in the serial path and a comes first. */
    [[nodiscard]] bool isBefore(const juce::String& a, const juce::String& b) const;
    /** Blocks and endpoints that can send to / receive from others. */
    [[nodiscard]] bool isValidEndpoint(const juce::String& id, bool asSource) const;

    /** Moves a block to a serial position (0 = first after the input). */
    void moveTo(const juce::String& id, int serialPosition);
    /** Moves a block directly in front of / behind another serial block. */
    void moveBefore(const juce::String& id, const juce::String& other);
    void moveAfter(const juce::String& id, const juce::String& other);
    /** The compressor as a pedal (in front of the wah) or in the studio position (after the graphic EQ). */
    [[nodiscard]] bool compressorBeforeAmp() const { return isBefore("compressor", "amp"); }
    void setCompressorBeforeAmp(bool beforeAmp) { beforeAmp ? moveBefore("compressor", "wah") : moveAfter("compressor", "geq"); }
    /** Takes a plugin or MIDI block out of the serial path (it keeps its links). */
    void moveToSide(const juce::String& id);
    /** Adds a block; returns its id. position < 0 = beside the chain. */
    juce::String add(Node node, int serialPosition);
    /** Removes a plugin or MIDI block and its links (built-ins stay). */
    void remove(const juce::String& id);
    void addLink(Link);
    void removeLink(const Link&);
    /** Drops links whose ends no longer exist or do not fit (MIDI to an audio-only block). */
    void sanitise();

    bool operator==(const Layout& other) const
    {
        return serial == other.serial && side == other.side && links == other.links && chainLevelDb == other.chainLevelDb;
    }
    bool operator!=(const Layout& other) const { return ! (*this == other); }
};
} // namespace chain
