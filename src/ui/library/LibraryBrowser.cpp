#include "LibraryBrowser.h"

namespace nam::ui
{
namespace
{
constexpr auto buttonHeight = 28;
}

LibraryBrowser::LibraryBrowser(LibraryManager& library, juce::String title)
    : manager(library), heading(title.isNotEmpty() ? std::move(title)
                                                   : manager.itemType().toUpperCase() + " Library")
{
    addAndMakeVisible(list);
    list.setColour(juce::ListBox::backgroundColourId, AmberTheme::surface());
    list.setColour(juce::ListBox::outlineColourId, AmberTheme::amber().withAlpha(0.4f));
    list.setRowHeight(38);

    auto addButton = [this](juce::TextButton& button, auto action)
    {
        addAndMakeVisible(button);
        button.setColour(juce::TextButton::buttonColourId, AmberTheme::surfaceRaised());
        button.setColour(juce::TextButton::textColourOffId, AmberTheme::text());
        button.onClick = action;
    };
    addButton(importButton, [this] { importFile(); });
    addButton(renameButton, [this] { renameSelected(); });
    addButton(tagsButton, [this] { editTags(); });
    addButton(missingButton, [this] { relinkFile(); });
    addButton(deleteButton, [this] { eraseSelected(); });
    deleteButton.setColour(juce::TextButton::buttonColourId, AmberTheme::danger().darker(0.35f));
    refresh();
}

LibraryBrowser::~LibraryBrowser() = default;

void LibraryBrowser::setCallbacks(Callbacks newCallbacks) { callbacks = std::move(newCallbacks); }

void LibraryBrowser::refresh()
{
    if (manager.find(selectedId) == nullptr)
        selectedId = {};
    list.updateContent();
    list.repaint();
    const auto hasSelection = selectedItem() != nullptr;
    renameButton.setEnabled(hasSelection);
    tagsButton.setEnabled(hasSelection);
    deleteButton.setEnabled(hasSelection);
    missingButton.setEnabled(hasSelection && selectedItem()->isMissing());
}

void LibraryBrowser::setSelectedItem(const juce::Uuid& id)
{
    for (int i = 0; i < manager.items().size(); ++i)
        if (manager.items().getReference(i).id == id)
        {
            list.selectRow(i, true);
            return;
        }
    list.deselectAllRows();
}

void LibraryBrowser::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    bounds.removeFromTop(24);
    auto controls = bounds.removeFromBottom(buttonHeight);
    const auto width = controls.getWidth() / 5;
    importButton.setBounds(controls.removeFromLeft(width).reduced(2, 0));
    renameButton.setBounds(controls.removeFromLeft(width).reduced(2, 0));
    tagsButton.setBounds(controls.removeFromLeft(width).reduced(2, 0));
    missingButton.setBounds(controls.removeFromLeft(width).reduced(2, 0));
    deleteButton.setBounds(controls.reduced(2, 0));
    bounds.removeFromBottom(6);
    list.setBounds(bounds);
}

void LibraryBrowser::paint(juce::Graphics& g)
{
    g.fillAll(AmberTheme::background());
    g.setColour(AmberTheme::text());
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText(heading, getLocalBounds().reduced(8).removeFromTop(24), juce::Justification::centredLeft);
}

int LibraryBrowser::getNumRows() { return manager.items().size(); }

void LibraryBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (!juce::isPositiveAndBelow(row, manager.items().size())) return;
    const auto& item = manager.items().getReference(row);
    if (selected) g.fillAll(AmberTheme::amber().withAlpha(0.22f));
    auto textArea = juce::Rectangle<int>(6, 2, width - 12, height - 4);
    g.setColour(item.isMissing() ? AmberTheme::danger() : AmberTheme::text());
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(item.displayName, textArea.removeFromTop(19), juce::Justification::centredLeft, true);
    g.setColour(AmberTheme::subduedText());
    g.setFont(juce::FontOptions(11.0f));
    auto subtext = item.isMissing() ? "Missing — relink required" : item.linkedFile.getFullPathName();
    if (!item.tags.isEmpty()) subtext << "  •  " << item.tags.joinIntoString(", ");
    g.drawText(subtext, textArea, juce::Justification::centredLeft, true);
}

void LibraryBrowser::selectedRowsChanged(int row)
{
    selectedId = juce::isPositiveAndBelow(row, manager.items().size()) ? manager.items().getReference(row).id : juce::Uuid{};
    refresh();
    if (const auto* item = selectedItem(); item != nullptr && callbacks.onSelect) callbacks.onSelect(*item);
}

void LibraryBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow(row, manager.items().size()) && manager.items().getReference(row).isMissing()) relinkFile();
}

const LibraryItem* LibraryBrowser::selectedItem() const noexcept { return manager.find(selectedId); }

juce::String LibraryBrowser::extensionWildcard() const
{
    return manager.itemType() == "nam" ? "*.nam" : "*.wav";
}

void LibraryBrowser::importFile()
{
    if (! callbacks.onImportFiles)
    {
        chooseImport(false);
        return;
    }
    juce::PopupMenu menu;
    menu.addItem(juce::String(juce::CharPointer_UTF8("Dateien importieren \xe2\x80\xa6 (Mehrfachauswahl)")), [this] { chooseImport(false); });
    menu.addItem(juce::String(juce::CharPointer_UTF8("Ordner importieren \xe2\x80\xa6 (inkl. Unterordner)")), [this] { chooseImport(true); });
    if (callbacks.extendImportMenu)
    {
        menu.addSeparator();
        callbacks.extendImportMenu(menu);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&importButton));
}

void LibraryBrowser::chooseImport(bool folders)
{
    chooser = std::make_unique<juce::FileChooser>("Import " + manager.itemType().toUpperCase(), juce::File{}, folders ? juce::String("*") : extensionWildcard());
    const auto flags = juce::FileBrowserComponent::openMode
                     | (folders ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles)
                     | juce::FileBrowserComponent::canSelectMultipleItems;
    chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        const auto files = fc.getResults();
        if (files.isEmpty() || files.getFirst() == juce::File{})
            return;
        if (callbacks.onImportFiles)
        {
            callbacks.onImportFiles(files);
            refresh();
            return;
        }
        juce::String error;
        std::optional<juce::Uuid> lastId;
        for (const auto& file : files)
        {
            if (const auto id = manager.importLink(file, {}, "{}", &error))
            {
                lastId = id;
                if (callbacks.onImport) callbacks.onImport(file);
            }
            else
                reportError(error);
        }
        refresh();
        if (lastId)
            setSelectedItem(*lastId);
    });
}

void LibraryBrowser::relinkFile()
{
    const auto* item = selectedItem();
    if (item == nullptr) return;
    const auto id = item->id;
    if (callbacks.onMissingAction) callbacks.onMissingAction(*item);
    chooser = std::make_unique<juce::FileChooser>("Relink " + item->displayName, item->linkedFile, extensionWildcard());
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, id](const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File{}) return;
        const auto* existing = manager.find(id);
        if (existing == nullptr) return;
        juce::String error;
        if (!manager.relink(id, file, &error)) { reportError(error); return; }
        if (callbacks.onRelink) callbacks.onRelink(*existing, file);
        refresh();
    });
}

void LibraryBrowser::showTextEditor(const juce::String& prompt, const juce::String& value,
                                    std::function<void(const juce::String&)> accepted)
{
    auto dialog = std::make_unique<juce::AlertWindow>(prompt, juce::String{}, juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("value", value);
    dialog->addButton("Cancel", 0);
    dialog->addButton("OK", 1);
    auto* raw = dialog.release();
    raw->enterModalState(true, juce::ModalCallbackFunction::create([raw, accepted = std::move(accepted)](int result)
    {
        if (result == 1) accepted(raw->getTextEditorContents("value"));
        delete raw;
    }), true);
}

void LibraryBrowser::renameSelected()
{
    const auto* item = selectedItem(); if (item == nullptr) return;
    const auto id = item->id;
    showTextEditor("Display name", item->displayName, [this, id](const juce::String& name)
    {
        const auto* original = manager.find(id); if (original == nullptr || name.trim().isEmpty()) return;
        if (manager.rename(id, name.trim())) { if (callbacks.onRename) callbacks.onRename(*original, name.trim()); refresh(); }
    });
}

void LibraryBrowser::editTags()
{
    const auto* item = selectedItem(); if (item == nullptr) return;
    const auto id = item->id;
    showTextEditor("Tags (comma separated)", item->tags.joinIntoString(", "), [this, id](const juce::String& value)
    {
        const auto* original = manager.find(id); if (original == nullptr) return;
        auto tags = juce::StringArray::fromTokens(value, ",", ""); tags.trim(); tags.removeEmptyStrings();
        if (manager.setTags(id, tags)) { if (callbacks.onTagsEdited) callbacks.onTagsEdited(*original, tags); refresh(); }
    });
}

void LibraryBrowser::eraseSelected()
{
    const auto* item = selectedItem(); if (item == nullptr) return;
    const auto copy = *item;
    if (manager.erase(copy.id)) { selectedId = {}; if (callbacks.onDelete) callbacks.onDelete(copy); refresh(); }
}

void LibraryBrowser::reportError(const juce::String& message)
{
    if (callbacks.onError) callbacks.onError(message);
    else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, heading, message);
}
} // namespace nam::ui
