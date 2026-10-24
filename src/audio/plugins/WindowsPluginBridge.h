#pragma once

#include <juce_core/juce_core.h>

/** Linux: runs Windows VST3 plugins through yabridge and Wine.

    The Linux package carries yabridge (next to the app in "yabridge") and
    Windows plugins (in "windows-plugins", e.g. Valhalla Supermassive).
    bridge() installs yabridge to ~/.local/share/yabridge, registers the
    folders with yabridgectl and syncs; the bridged plugins then appear as
    ordinary Linux VST3 plugins in ~/.vst3/yabridge. Wine (Staging) must be
    installed by the user. On Windows everything here is a no-op. */
namespace windowsbridge
{
[[nodiscard]] bool isSupportedPlatform();
/** At startup: environment for Wine started by bridged plugins (no Mono/Gecko prompts). */
void prepareEnvironment();
/** "wine-9.21 (Staging)" or empty if Wine is not installed. Blocking (runs wine --version). */
[[nodiscard]] juce::String wineVersion();
/** Windows plugins shipped with the app. */
[[nodiscard]] juce::File bundledPluginFolder();
/** Where bridged plugins appear (inside ~/.vst3, which the scan covers). */
[[nodiscard]] juce::File bridgedPluginFolder();
/** Blocking (worker thread): installs yabridge if needed, registers the bundled
    folder and the given ones and runs yabridgectl sync. Returns the error, or an
    empty string on success. */
[[nodiscard]] juce::String bridge(const juce::StringArray& extraFolders);
/** Stops bridging a folder and removes its bridges (yabridgectl rm, sync --prune). Blocking, short. */
void unbridge(const juce::String& folder);
/** Whether a plugin file is a yabridge bridge (loads slowly the first time: Wine starts). */
[[nodiscard]] bool isBridged(const juce::String& pluginFile);
} // namespace windowsbridge
