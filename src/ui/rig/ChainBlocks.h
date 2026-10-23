#pragma once

#include <functional>

#include "SignalChainStrip.h"
#include "presets/PresetManager.h"

class RigChain;

/** What the signal chain shows for each block, shared by the live rig and
    the track effect chains, so both describe and switch blocks the same way. */
namespace nam::ui::chainblocks
{
struct Context
{
    juce::String ampName, irName;       // display names of the loaded NAM model and IR
    bool pedalLoaded = false, pedalOn = false;
    juce::String pedalName;
    RigChain* rig = nullptr;            // for the plugins' loaded names
};

[[nodiscard]] SignalChainStrip::Block describe(const chain::Node&, const SoundState&, const Context&);

/** Flips the on/off switch of a built-in block in the sound; false if the
    block has none there (plugins and MIDI tools switch in the layout). */
bool toggleBuiltin(SoundState&, const juce::String& kind);

/** Flips a plugin or MIDI block in the layout and in the running chain. */
void togglePluginBlock(SoundState&, RigChain&, const juce::String& id);

/** A window with the plugin's own editor, or null (error says why). Close it
    before the block is removed: editors must go before their plugin. */
[[nodiscard]] std::unique_ptr<juce::DocumentWindow> openPluginWindow(RigChain&, const juce::String& id, juce::String& error);
} // namespace nam::ui::chainblocks
