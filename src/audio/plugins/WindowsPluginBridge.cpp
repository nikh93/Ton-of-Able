#include "WindowsPluginBridge.h"

#include <cstdlib>

namespace windowsbridge
{
namespace
{
juce::String utf8(const char* text) { return juce::String(juce::CharPointer_UTF8(text)); }

juce::File appDirectory()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
}

juce::File installedYabridge()
{
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".local/share/yabridge");
}

/** Runs a command; returns its output, or sets failed when it could not run or exited with an error. */
juce::String run(const juce::StringArray& command, int timeoutMs, bool& failed)
{
    juce::ChildProcess process;
    failed = true;
    if (! process.start(command))
        return {};
    const auto output = process.readAllProcessOutput();
    if (! process.waitForProcessToFinish(timeoutMs))
    {
        process.kill();
        return output + utf8("\n(Zeitüberschreitung)");
    }
    failed = process.getExitCode() != 0;
    return output;
}

/** Copies the yabridge shipped with the app to ~/.local/share/yabridge, where
    the bridged plugins look for it, unless the same files are there already. */
juce::String installYabridge()
{
    const auto source = appDirectory().getChildFile("yabridge");
    const auto target = installedYabridge();
    if (! source.getChildFile("yabridgectl").existsAsFile())
        return target.getChildFile("yabridgectl").existsAsFile() ? juce::String() : utf8("yabridge fehlt im Programmordner.");
    const auto same = [&](const juce::String& name)
    {
        const auto a = source.getChildFile(name), b = target.getChildFile(name);
        return b.existsAsFile() && a.getSize() == b.getSize() && a.hashCode64() == b.hashCode64();
    };
    if (same("yabridgectl") && same("libyabridge-vst3.so") && same("yabridge-host.exe.so"))
        return {};
    if (! target.createDirectory() || ! source.copyDirectoryTo(target))
        return utf8("yabridge ließ sich nicht nach ") + target.getFullPathName() + utf8(" kopieren.");
    for (const auto& file : target.findChildFiles(juce::File::findFiles, false))
        if (! file.hasFileExtension("md"))
            file.setExecutePermission(true);
    return {};
}

/** yabridge links to the Windows plugin files. An AppImage is mounted at a new
    path on every start, so the shipped plugins are copied to a fixed place first. */
juce::File stableBundledPlugins()
{
    const auto source = bundledPluginFolder();
    const auto target = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".local/share/ton-of-able/windows-plugins");
    for (const auto& plugin : source.findChildFiles(juce::File::findFilesAndDirectories, false, "*.vst3"))
    {
        const auto copy = target.getChildFile(plugin.getFileName());
        if (copy.exists() && copy.getSize() == plugin.getSize() && copy.getLastModificationTime() >= plugin.getLastModificationTime())
            continue;
        target.createDirectory();
        copy.deleteRecursively();
        if (plugin.isDirectory())
            plugin.copyDirectoryTo(copy);
        else
            plugin.copyFileTo(copy);
    }
    return target;
}

/** Supermassive reads its factory presets from ProgramData inside the Wine prefix. */
void installSupermassivePresets()
{
    const auto prefix = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".wine");
    const auto presets = appDirectory().getChildFile("Supermassive-Presets").getChildFile("Factory");
    if (! prefix.getChildFile("system.reg").existsAsFile() || ! presets.isDirectory())
        return;
    const auto target = prefix.getChildFile("drive_c/ProgramData/Valhalla DSP, LLC/ValhallaSupermassive/Presets/Factory");
    if (! target.isDirectory())
        presets.copyDirectoryTo(target);
}
} // namespace

void prepareEnvironment()
{
   #if JUCE_LINUX
    // A new Wine prefix would otherwise stop to offer installing Mono and Gecko,
    // which audio plugins do not need.
    if (juce::SystemStats::getEnvironmentVariable("WINEDLLOVERRIDES", {}).isEmpty())
        setenv("WINEDLLOVERRIDES", "mscoree=d;mshtml=d", 1);
   #endif
}

bool isSupportedPlatform()
{
   #if JUCE_LINUX
    return true;
   #else
    return false;
   #endif
}

juce::String wineVersion()
{
    if (! isSupportedPlatform())
        return {};
    bool failed = false;
    const auto output = run({ "wine", "--version" }, 10000, failed);
    if (failed)
        return {};
    // Wine may print warnings (e.g. about 32-bit support) around the version line.
    for (const auto& line : juce::StringArray::fromLines(output))
        if (line.trim().startsWith("wine-"))
            return line.trim();
    return output.trim().upToFirstOccurrenceOf("\n", false, false);
}

juce::File bundledPluginFolder() { return appDirectory().getChildFile("windows-plugins"); }

juce::File bridgedPluginFolder()
{
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3/yabridge");
}

void unbridge(const juce::String& folder)
{
    const auto yabridgectl = installedYabridge().getChildFile("yabridgectl");
    if (! isSupportedPlatform() || ! yabridgectl.existsAsFile())
        return;
    bool failed = false;
    run({ yabridgectl.getFullPathName(), "rm", folder }, 20000, failed);
    run({ yabridgectl.getFullPathName(), "sync", "--no-verify", "--prune" }, 60000, failed);
}

bool isBridged(const juce::String& pluginFile)
{
    return isSupportedPlatform() && juce::File(pluginFile).isAChildOf(bridgedPluginFolder());
}

juce::String bridge(const juce::StringArray& extraFolders)
{
    if (! isSupportedPlatform())
        return {};
    juce::StringArray folders(extraFolders);
    const auto hasBundled = bundledPluginFolder().isDirectory();
    folders.removeEmptyStrings();
    if (folders.isEmpty() && ! hasBundled)
        return {};
    if (wineVersion().isEmpty())
        return utf8("Für Windows-Plugins wird Wine benötigt (am besten Wine Staging, z. B. „sudo apt install wine-staging“ "
                    "aus den WineHQ-Paketquellen).");
    if (const auto error = installYabridge(); error.isNotEmpty())
        return error;
    if (hasBundled)
        folders.insert(0, stableBundledPlugins().getFullPathName());
    const auto yabridgectl = installedYabridge().getChildFile("yabridgectl").getFullPathName();
    bool failed = false;
    for (const auto& folder : folders)
        if (juce::File(folder).isDirectory())
            run({ yabridgectl, "add", folder }, 20000, failed);
    // Without the post-install check, so sync never waits on Wine creating its prefix;
    // that happens when a bridged plugin loads for the first time.
    const auto output = run({ yabridgectl, "sync", "--no-verify" }, 120000, failed);
    if (failed)
        return utf8("yabridgectl sync meldet einen Fehler:\n") + output.trim().getLastCharacters(800);
    installSupermassivePresets();
    return {};
}
} // namespace windowsbridge
