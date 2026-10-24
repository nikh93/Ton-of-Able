#include "PluginManagerComponent.h"

#include "audio/plugins/WindowsPluginBridge.h"
#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using daw::text;
using Theme = AmberTheme;

namespace
{
enum Column { shown = 1, name, maker, kind, file };
constexpr int removeWidth = 28;
} // namespace

PluginManagerComponent::PluginManagerComponent(Host h) : host(std::move(h))
{
    title.setText(text("VST3-Plugins"), juce::dontSendNotification);
    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    foldersHeading.setText(text("SUCHORDNER"), juce::dontSendNotification);
    pluginsHeading.setText(text("PLUGINS  ·  Haken = im „+“-Menü angeboten, Rechtsklick für mehr"), juce::dontSendNotification);
    for (auto* label : { &foldersHeading, &pluginsHeading })
    {
        label->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, Theme::subduedText());
    }
    status.setColour(juce::Label::textColourId, Theme::subduedText());
    importButton.setButtonText(text("Plugin-Datei importieren …"));
    importButton.setTooltip(text("Eine oder mehrere .vst3-Dateien hinzufügen, egal wo sie liegen"));
    folderButton.setButtonText(text("Ordner hinzufügen …"));
    folderButton.setTooltip(text("Einen Ordner (mit Unterordnern) bei jeder Suche nach Plugins durchsuchen"));
    rescanButton.setButtonText(text("Alle neu suchen"));
    rescanButton.setTooltip(text("Alle Ordner und importierten Dateien neu prüfen (jedes Plugin in einem eigenen Prozess)"));
    windowsButton.setButtonText(text("Windows-Plugins …"));
    windowsButton.setTooltip(text("Einen Ordner mit Windows-VST3-Plugins über yabridge und Wine einbinden"));
    windowsButton.onClick = [this] { addWindowsFolder(); };
    bridgeInfo.setColour(juce::Label::textColourId, Theme::subduedText());
    bridgeInfo.setFont(juce::FontOptions(12.5f));
    importButton.onClick = [this] { importFiles(); };
    folderButton.onClick = [this] { addFolder(); };
    rescanButton.onClick = [this] { scan({}, text("Alle Plugins")); };

    folderList.setModel(&folders);
    folderList.setRowHeight(22);
    table.setModel(this);
    table.setRowHeight(24);
    auto& header = table.getHeader();
    const auto flags = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable;
    header.addColumn(text("Menü"), shown, 52, 52, 52, juce::TableHeaderComponent::visible);
    header.addColumn(text("Name"), name, 190, 80, 400, flags);
    header.addColumn(text("Hersteller"), maker, 130, 60, 300, flags);
    header.addColumn(text("Art"), kind, 90, 60, 160, flags);
    header.addColumn(text("Datei / Problem"), file, 320, 100, 1200, flags);
    header.setStretchToFitActive(true);
    header.setColour(juce::TableHeaderComponent::backgroundColourId, Theme::panel());
    header.setColour(juce::TableHeaderComponent::textColourId, Theme::subduedText());
    header.setColour(juce::TableHeaderComponent::outlineColourId, Theme::outline());
    header.setColour(juce::TableHeaderComponent::highlightColourId, Theme::control());
    for (auto* list : { static_cast<juce::ListBox*>(&folderList), static_cast<juce::ListBox*>(&table) })
    {
        list->setColour(juce::ListBox::backgroundColourId, Theme::well());
        list->setColour(juce::ListBox::outlineColourId, Theme::outline());
        list->setOutlineThickness(1);
    }
    for (juce::Component* c : std::initializer_list<juce::Component*> { &title, &status, &importButton, &folderButton, &rescanButton,
                                                                         &foldersHeading, &pluginsHeading, &folderList, &table })
        addAndMakeVisible(*c);
    // Linux only: Windows plugins through yabridge.
    addChildComponent(windowsButton);
    addChildComponent(bridgeInfo);
    windowsButton.setVisible(windowsbridge::isSupportedPlatform());
    bridgeInfo.setVisible(windowsbridge::isSupportedPlatform());
    setSize(860, 560);
    refresh();
}

void PluginManagerComponent::rebuildFolderRows()
{
    const auto& catalog = host.catalog();
    folderRows.clear();
    juce::StringArray seen;
    for (const auto& folder : PluginCatalog::standardFolders())
        if (folder.isDirectory() && ! seen.contains(folder.getFullPathName()))
        {
            seen.add(folder.getFullPathName());
            folderRows.push_back({ FolderRow::Kind::standard, folder.getFullPathName() });
        }
    for (const auto& folder : catalog.folders)
        folderRows.push_back({ FolderRow::Kind::user, folder });
    for (const auto& folder : catalog.windowsFolders)
        folderRows.push_back({ FolderRow::Kind::windows, folder });
}

void PluginManagerComponent::refresh()
{
    const auto& catalog = host.catalog();
    rows.clear();
    std::vector<int> order;
    for (int index = 0; index < catalog.plugins.size(); ++index)
        order.push_back(index);
    std::sort(order.begin(), order.end(), [&catalog](int a, int b)
    {
        return catalog.plugins.getReference(a).name.compareNatural(catalog.plugins.getReference(b).name) < 0;
    });
    // Problems first: they are what the user opens this window for.
    for (size_t index = 0; index < catalog.failures.size(); ++index)
        rows.push_back({ -1, static_cast<int>(index) });
    for (const auto index : order)
        rows.push_back({ index, -1 });
    rebuildFolderRows();
    folderList.updateContent();
    folderList.repaint();
    table.updateContent();
    table.repaint();

    const auto busy = host.scanning();
    for (auto* button : { &importButton, &folderButton, &rescanButton, &windowsButton })
        button->setEnabled(! busy);
    if (windowsbridge::isSupportedPlatform())
    {
        const auto& bridge = catalog.bridgeStatus;
        bridgeInfo.setText(bridge.isNotEmpty() ? bridge
                           : text("Windows-Plugins (z. B. das mitgelieferte Supermassive) laufen über yabridge und Wine. "
                                  "Sie werden bei „Alle neu suchen“ eingerichtet."),
                           juce::dontSendNotification);
        const auto problem = bridge.isNotEmpty() && ! bridge.startsWith(text("Windows-Plugins über yabridge eingerichtet"));
        bridgeInfo.setColour(juce::Label::textColourId, problem ? Theme::danger() : Theme::subduedText());
    }
    if (busy)
        status.setText(text("Suche läuft … (jedes Plugin wird in einem eigenen Prozess geprüft)"), juce::dontSendNotification);
    else
    {
        int instruments = 0, hiddenCount = 0;
        for (const auto& description : catalog.plugins)
        {
            instruments += description.isInstrument ? 1 : 0;
            hiddenCount += catalog.isHidden(description) ? 1 : 0;
        }
        auto summary = juce::String(catalog.plugins.size()) + text(" Plugins (") + juce::String(instruments) + text(" Instrumente)");
        if (hiddenCount > 0)
            summary << text(", ") << hiddenCount << text(" ausgeblendet");
        if (! catalog.failures.empty())
            summary << text(", ") << static_cast<int>(catalog.failures.size()) << text(" fehlerhaft");
        status.setText(summary, juce::dontSendNotification);
    }
}

void PluginManagerComponent::paint(juce::Graphics& g)
{
    g.fillAll(Theme::surface());
}

void PluginManagerComponent::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto top = area.removeFromTop(30);
    rescanButton.setBounds(top.removeFromRight(130));
    if (windowsButton.isVisible())
    {
        top.removeFromRight(8);
        windowsButton.setBounds(top.removeFromRight(150));
    }
    top.removeFromRight(8);
    folderButton.setBounds(top.removeFromRight(160));
    top.removeFromRight(8);
    importButton.setBounds(top.removeFromRight(200));
    title.setBounds(top);
    area.removeFromTop(10);
    foldersHeading.setBounds(area.removeFromTop(20));
    const auto rowsShown = juce::jlimit(2, 6, static_cast<int>(folderRows.size()));
    folderList.setBounds(area.removeFromTop(rowsShown * folderList.getRowHeight() + 2));
    if (bridgeInfo.isVisible())
        bridgeInfo.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);
    status.setBounds(area.removeFromBottom(24));
    area.removeFromBottom(4);
    pluginsHeading.setBounds(area.removeFromTop(20));
    table.setBounds(area);
}

//==============================================================================
int PluginManagerComponent::Folders::getNumRows()
{
    return static_cast<int>(owner.folderRows.size());
}

void PluginManagerComponent::Folders::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool)
{
    if (row < 0 || row >= static_cast<int>(owner.folderRows.size()))
        return;
    const auto& entry = owner.folderRows[static_cast<size_t>(row)];
    const auto& path = entry.path;
    auto area = juce::Rectangle<int>(0, 0, width, height).reduced(8, 0);
    g.setFont(juce::FontOptions(13.0f));
    if (entry.kind == FolderRow::Kind::standard)
    {
        g.setColour(Theme::subduedText());
        g.drawText(text("Standard"), area.removeFromRight(70), juce::Justification::centredRight);
    }
    else
    {
        g.setColour(Theme::danger());
        g.drawText(text("✕"), area.removeFromRight(removeWidth), juce::Justification::centred);
        if (entry.kind == FolderRow::Kind::windows)
        {
            g.setColour(Theme::amber());
            g.drawText(text("Windows · yabridge"), area.removeFromRight(130), juce::Justification::centredRight);
        }
    }
    g.setColour(juce::File(path).isDirectory() ? Theme::text() : Theme::danger());
    g.drawText(path, area, juce::Justification::centredLeft, true);
}

void PluginManagerComponent::Folders::listBoxItemClicked(int row, const juce::MouseEvent& event)
{
    if (row < 0 || row >= static_cast<int>(owner.folderRows.size()) || owner.host.scanning()
        || event.x < owner.folderList.getVisibleRowWidth() - removeWidth - 8)
        return;
    const auto entry = owner.folderRows[static_cast<size_t>(row)];
    auto& catalog = owner.host.catalog();
    if (entry.kind == FolderRow::Kind::standard)
        return;
    if (entry.kind == FolderRow::Kind::user)
        catalog.removeFolder(entry.path);
    else
    {
        catalog.windowsFolders.removeString(entry.path);
        windowsbridge::unbridge(entry.path);
        // Their bridges are gone from ~/.vst3/yabridge: a full search drops them.
        owner.scan({}, text("Alle Plugins"));
    }
    owner.host.changed();
    owner.refresh();
    owner.resized();
}

//==============================================================================
int PluginManagerComponent::getNumRows() { return static_cast<int>(rows.size()); }

void PluginManagerComponent::paintRowBackground(juce::Graphics& g, int row, int, int, bool selected)
{
    if (selected)
        g.fillAll(Theme::amber().withAlpha(0.18f));
    else if (row % 2 == 1)
        g.fillAll(juce::Colours::white.withAlpha(0.025f));
}

juce::String PluginManagerComponent::fileOf(const Row& row) const
{
    const auto& catalog = host.catalog();
    return row.plugin >= 0 ? catalog.plugins.getReference(row.plugin).fileOrIdentifier
                           : catalog.failures[static_cast<size_t>(row.failure)].file;
}

void PluginManagerComponent::paintCell(juce::Graphics& g, int rowNumber, int column, int width, int height, bool)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(rows.size()))
        return;
    const auto& row = rows[static_cast<size_t>(rowNumber)];
    const auto& catalog = host.catalog();
    const auto area = juce::Rectangle<int>(0, 0, width, height).reduced(6, 0);
    g.setFont(juce::FontOptions(13.0f));
    if (row.failure >= 0)
    {
        const auto& failure = catalog.failures[static_cast<size_t>(row.failure)];
        g.setColour(Theme::danger());
        if (column == name) g.drawText(juce::File(failure.file).getFileNameWithoutExtension(), area, juce::Justification::centredLeft, true);
        if (column == kind) g.drawText(text("Fehler"), area, juce::Justification::centredLeft, true);
        if (column == file) g.drawText(failure.reason, area, juce::Justification::centredLeft, true);
        return;
    }
    const auto& description = catalog.plugins.getReference(row.plugin);
    const auto hidden = catalog.isHidden(description);
    if (column == shown)
    {
        const auto box = juce::Rectangle<float>(14.0f, 14.0f).withCentre(area.toFloat().getCentre());
        g.setColour(Theme::outline().withAlpha(0.4f));
        g.drawRoundedRectangle(box, 3.0f, 1.2f);
        if (! hidden)
        {
            g.setColour(Theme::amber());
            g.fillRoundedRectangle(box.reduced(3.0f), 2.0f);
        }
        return;
    }
    g.setColour(hidden ? Theme::subduedText().withAlpha(0.6f) : column == name ? Theme::text() : Theme::subduedText());
    const auto value = column == name ? description.name
                     : column == maker ? description.manufacturerName
                     : column == kind ? (description.isInstrument ? text("Instrument") : text("Effekt"))
                     : description.fileOrIdentifier;
    g.drawText(value, area, juce::Justification::centredLeft, true);
}

juce::String PluginManagerComponent::getCellTooltip(int rowNumber, int column)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(rows.size()))
        return {};
    const auto& row = rows[static_cast<size_t>(rowNumber)];
    if (row.failure >= 0)
        return fileOf(row) + "\n" + host.catalog().failures[static_cast<size_t>(row.failure)].reason;
    if (column == shown)
        return text("Im „+“-Menü der Signalkette und bei den Drum-Instrumenten anbieten");
    return fileOf(row);
}

void PluginManagerComponent::cellClicked(int rowNumber, int column, const juce::MouseEvent& event)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(rows.size()))
        return;
    if (event.mods.isPopupMenu())
    {
        showRowMenu(rowNumber);
        return;
    }
    const auto& row = rows[static_cast<size_t>(rowNumber)];
    if (column == shown && row.plugin >= 0)
    {
        auto& catalog = host.catalog();
        const auto& description = catalog.plugins.getReference(row.plugin);
        catalog.setHidden(description, ! catalog.isHidden(description));
        host.changed();
        refresh();
    }
}

void PluginManagerComponent::showRowMenu(int rowNumber)
{
    const auto row = rows[static_cast<size_t>(rowNumber)];
    const auto path = fileOf(row);
    auto& catalog = host.catalog();
    const auto imported = catalog.files.contains(path);
    juce::PopupMenu menu;
    if (row.plugin >= 0)
        menu.addItem(1, text("Im Menü anbieten"), true, ! catalog.isHidden(catalog.plugins.getReference(row.plugin)));
    menu.addItem(2, text("Erneut prüfen"), ! host.scanning() && juce::File(path).exists());
    menu.addItem(3, text("Im Dateimanager zeigen"), juce::File(path).exists());
    menu.addSeparator();
    menu.addItem(4, imported ? text("Import entfernen") : text("Liegt in einem Suchordner – dort löschen oder ausblenden"), imported && ! host.scanning());
    const juce::Component::SafePointer<PluginManagerComponent> self(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(), [self, row, path](int result)
    {
        if (self == nullptr || result == 0)
            return;
        auto& catalog = self->host.catalog();
        if (result == 1 && row.plugin >= 0 && row.plugin < catalog.plugins.size())
        {
            const auto& description = catalog.plugins.getReference(row.plugin);
            catalog.setHidden(description, ! catalog.isHidden(description));
        }
        else if (result == 2)
        {
            self->scan({ path }, juce::File(path).getFileNameWithoutExtension());
            return;
        }
        else if (result == 3)
        {
            juce::File(path).revealToUser();
            return;
        }
        else if (result == 4)
            catalog.removeFile(path);
        self->host.changed();
        self->refresh();
    });
}

void PluginManagerComponent::scan(const juce::StringArray& files, const juce::String& what)
{
    if (host.scanning())
        return;
    const juce::Component::SafePointer<PluginManagerComponent> self(this);
    host.scan(files, [self, what]
    {
        if (self == nullptr)
            return;
        self->refresh();
        self->status.setText(what + text(" geprüft. ") + self->status.getText(), juce::dontSendNotification);
    });
    refresh();
}

void PluginManagerComponent::importFiles()
{
    chooser = std::make_shared<juce::FileChooser>(text("VST3-Plugin importieren"), juce::File(), "*.vst3");
    const juce::Component::SafePointer<PluginManagerComponent> self(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectMultipleItems,
                         [self](const juce::FileChooser& fc)
    {
        if (self == nullptr || fc.getResults().isEmpty())
            return;
        juce::StringArray added;
        for (const auto& chosen : fc.getResults())
        {
            const auto bundle = PluginCatalog::bundleOf(chosen).getFullPathName();
            self->host.catalog().files.addIfNotAlreadyThere(bundle);
            added.addIfNotAlreadyThere(bundle);
        }
        self->host.changed();
        self->scan(added, added.size() == 1 ? juce::File(added[0]).getFileNameWithoutExtension() : juce::String(added.size()) + text(" Dateien"));
    });
}

void PluginManagerComponent::addFolder()
{
    chooser = std::make_shared<juce::FileChooser>(text("Ordner mit VST3-Plugins wählen"));
    const juce::Component::SafePointer<PluginManagerComponent> self(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [self](const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (self == nullptr || ! folder.isDirectory())
            return;
        // A chosen bundle is a plugin, not a folder of plugins.
        if (folder.hasFileExtension("vst3"))
        {
            self->host.catalog().files.addIfNotAlreadyThere(folder.getFullPathName());
            self->host.changed();
            self->scan({ folder.getFullPathName() }, folder.getFileNameWithoutExtension());
            return;
        }
        self->host.catalog().folders.addIfNotAlreadyThere(folder.getFullPathName());
        self->host.changed();
        self->resized();
        juce::StringArray found;
       #if JUCE_PLUGINHOST_VST3
        found = juce::VST3PluginFormat().searchPathsForPlugins(juce::FileSearchPath(folder.getFullPathName()), true, false);
       #endif
        if (found.isEmpty())
        {
            self->refresh();
            self->status.setText(text("In diesem Ordner liegen keine .vst3-Dateien."), juce::dontSendNotification);
            return;
        }
        self->scan(found, folder.getFileName());
    });
}
void PluginManagerComponent::addWindowsFolder()
{
    chooser = std::make_shared<juce::FileChooser>(text("Ordner mit Windows-VST3-Plugins wählen (z. B. in ~/.wine/drive_c/Program Files/Common Files/VST3)"),
                                                  juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                                                      .getChildFile(".wine/drive_c/Program Files/Common Files/VST3"));
    const juce::Component::SafePointer<PluginManagerComponent> self(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [self](const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (self == nullptr || ! folder.isDirectory())
            return;
        self->host.catalog().windowsFolders.addIfNotAlreadyThere(folder.getFullPathName());
        self->host.changed();
        self->refresh();
        self->resized();
        // Bridging happens with the full search.
        self->scan({}, text("Windows-Plugins"));
    });
}
} // namespace nam::ui
