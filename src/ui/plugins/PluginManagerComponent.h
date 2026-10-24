#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "audio/plugins/PluginCatalog.h"

namespace nam::ui
{
/** Manages the VST3 plugins: search folders, imported plugin files, plugins
    hidden from the menus and the ones that failed to load. */
class PluginManagerComponent final : public juce::Component, private juce::TableListBoxModel
{
public:
    struct Host
    {
        std::function<PluginCatalog&()> catalog;
        /** After an edit without a scan (hidden, removed): store it, refresh the menus. */
        std::function<void()> changed;
        /** Scans these files (empty: everything) and calls back when done. */
        std::function<void(const juce::StringArray& files, std::function<void()> done)> scan;
        std::function<bool()> scanning;
    };

    explicit PluginManagerComponent(Host);
    /** Rebuilds the lists from the catalog (e.g. after a scan started elsewhere). */
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        int plugin = -1;  // index into the catalog's plugins, or
        int failure = -1; // into its failures
    };
    // Plugin table.
    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell(juce::Graphics&, int row, int column, int width, int height, bool selected) override;
    void cellClicked(int row, int column, const juce::MouseEvent&) override;
    juce::String getCellTooltip(int row, int column) override;
    struct Folders final : juce::ListBoxModel
    {
        explicit Folders(PluginManagerComponent& o) : owner(o) {}
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
        PluginManagerComponent& owner;
    };

    void showRowMenu(int row);
    void importFiles();
    void addFolder();
    void scan(const juce::StringArray& files, const juce::String& what);
    [[nodiscard]] juce::String fileOf(const Row&) const;
    void addWindowsFolder();
    void rebuildFolderRows();

    struct FolderRow
    {
        enum class Kind { standard, user, windows } kind;
        juce::String path;
    };

    Host host;
    std::vector<Row> rows;
    juce::Label title, status;
    juce::TextButton importButton, folderButton, rescanButton, windowsButton;
    juce::Label foldersHeading, pluginsHeading, bridgeInfo;
    std::vector<FolderRow> folderRows;
    Folders folders { *this };
    juce::ListBox folderList;
    juce::TableListBox table;
    std::shared_ptr<juce::FileChooser> chooser;
};
} // namespace nam::ui
