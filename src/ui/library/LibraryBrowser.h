#pragma once

#include "../../library/LibraryManager.h"
#include "../components/AmberTheme.h"

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/**
    Reusable browser for a LibraryManager-derived NAM or IR library.

    The component deliberately owns no library data.  It calls the supplied
    LibraryManager for mutations and reports each user action through callbacks,
    allowing an application to persist state, prepare DSP resources, or replace
    the built-in UI workflow.  Linked source files are never deleted.
*/
class LibraryBrowser final : public juce::Component,
                             private juce::ListBoxModel
{
public:
    struct Callbacks
    {
        std::function<void(const LibraryItem&)> onSelect;
        std::function<void(const juce::File&)> onImport;
        std::function<void(const LibraryItem&, const juce::String&)> onRename;
        std::function<void(const LibraryItem&)> onDelete;
        std::function<void(const LibraryItem&, const juce::StringArray&)> onTagsEdited;
        std::function<void(const LibraryItem&)> onMissingAction;
        std::function<void(const LibraryItem&, const juce::File&)> onRelink;
        std::function<void(const juce::String&)> onError;
        /** If set, the browser hands selected files/folders to the app (which
            copies them into the model folder) instead of linking them directly. */
        std::function<void(const juce::Array<juce::File>&)> onImportFiles;
        /** Adds app-level entries (model folder actions) to the import menu. */
        std::function<void(juce::PopupMenu&)> extendImportMenu;
    };

    explicit LibraryBrowser(LibraryManager& library, juce::String title = {});
    ~LibraryBrowser() override;

    void setCallbacks(Callbacks callbacks);
    void refresh();
    void setSelectedItem(const juce::Uuid& id);
    const juce::Uuid& selectedItemId() const noexcept { return selectedId; }

    void resized() override;
    void paint(juce::Graphics&) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    const LibraryItem* selectedItem() const noexcept;
    void importFile();
    void chooseImport(bool folders);
    void relinkFile();
    void renameSelected();
    void editTags();
    void eraseSelected();
    void showTextEditor(const juce::String& heading, const juce::String& initialText,
                        std::function<void(const juce::String&)> accepted);
    void reportError(const juce::String& message);
    juce::String extensionWildcard() const;

    LibraryManager& manager;
    Callbacks callbacks;
    juce::String heading;
    juce::ListBox list { "library items", this };
    juce::TextButton importButton { "Importieren" };
    juce::TextButton renameButton { "Umbenennen" };
    juce::TextButton tagsButton { "Tags" };
    juce::TextButton missingButton; // "Neu verknüpfen", set in the constructor (UTF-8)
    juce::TextButton deleteButton { "Entfernen" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Uuid selectedId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryBrowser)
};
} // namespace nam::ui
