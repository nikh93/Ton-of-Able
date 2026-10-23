#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** The VST3 plugins the app knows: where it looks for them, what the last scan
    found or could not load, and which plugins the user hid from the menus.
    The scan itself runs each plugin file in a child process ("--scan-vst3-all"),
    so a crashing plugin cannot take the app down. */
class PluginCatalog
{
public:
    struct Failure
    {
        juce::String file, reason;
    };
    struct ScanResult
    {
        juce::Array<juce::PluginDescription> found;
        std::vector<Failure> failures;
    };

    /** Folders searched without asking: the app folder, its "plugins" folder and the system VST3 folders. */
    [[nodiscard]] static juce::Array<juce::File> standardFolders();
    /** Every plugin file to scan: those in the standard and user folders plus imported files. */
    [[nodiscard]] juce::StringArray filesToScan() const;
    /** A chosen file inside a VST3 bundle stands for the bundle (".../X.vst3/Contents/..." → ".../X.vst3"). */
    [[nodiscard]] static juce::File bundleOf(const juce::File&);

    /** Blocking: scans the files one after another in child processes (worker thread). */
    [[nodiscard]] static ScanResult scan(const juce::StringArray& files);
    /** Takes a scan result: the scanned files' old entries and failures are replaced. */
    void merge(const juce::StringArray& scannedFiles, const ScanResult&);

    /** Forgets an imported file or user folder (and the plugins found there). */
    void removeFile(const juce::String& file);
    void removeFolder(const juce::String& folder);
    void setHidden(const juce::PluginDescription&, bool hidden);
    [[nodiscard]] bool isHidden(const juce::PluginDescription&) const;
    /** The plugins offered in the menus (not hidden). */
    [[nodiscard]] juce::Array<juce::PluginDescription> visible() const;
    [[nodiscard]] juce::Array<juce::PluginDescription> instruments() const;

    [[nodiscard]] juce::ValueTree toValueTree() const;
    /** Also reads the plain plugin list ("vst3Catalog") of earlier versions. */
    [[nodiscard]] static PluginCatalog fromUiState(const juce::ValueTree& uiState);

    juce::StringArray folders;                // added by the user
    juce::StringArray files;                  // single imported plugin files
    juce::StringArray hidden;                 // plugin identifiers left out of the menus
    juce::Array<juce::PluginDescription> plugins;
    std::vector<Failure> failures;
};
