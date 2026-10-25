#include "ChordProgressionEditor.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using daw::text;
using Theme = AmberTheme;
namespace harmony = daw::harmony;

class ChordProgressionEditor::SectionList final : public juce::ListBoxModel
{
public:
    explicit SectionList(ChordProgressionEditor& e) : editor(e) {}
    int getNumRows() override { return static_cast<int>(editor.sections.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow(row, getNumRows()))
            return;
        const auto& section = editor.sections[static_cast<size_t>(row)];
        g.fillAll(selected ? Theme::amber().withAlpha(0.25f) : (row % 2 == 0 ? Theme::well() : Theme::panel()));
        auto area = juce::Rectangle<int>(0, 0, width, height).reduced(8, 2);
        g.setColour(Theme::amberBright());
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(section.name, area.removeFromLeft(130), juce::Justification::centredLeft, true);
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(text("Takt ") + juce::String(section.firstBar + 1) + text("–") + juce::String(section.firstBar + section.bars),
                   area.removeFromLeft(90), juce::Justification::centredLeft, true);
        g.setColour(Theme::text());
        g.drawText(section.text.isEmpty() ? text("(leer)") : section.text, area, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged(int row) override { editor.selectSection(row); }

private:
    ChordProgressionEditor& editor;
};

class ChordProgressionEditor::LibraryList final : public juce::ListBoxModel
{
public:
    explicit LibraryList(ChordProgressionEditor& e) : editor(e) {}
    int getNumRows() override { return static_cast<int>(entries.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow(row, getNumRows()))
            return;
        const auto& entry = *entries[static_cast<size_t>(row)];
        const auto key = editor.key();
        g.fillAll(selected ? Theme::amber().withAlpha(0.25f) : (row % 2 == 0 ? Theme::well() : Theme::panel()));
        auto area = juce::Rectangle<int>(0, 0, width, height).reduced(8, 3);
        auto top = area.removeFromTop(18);
        g.setColour(Theme::amberBright());
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        const auto kind = entry.kind == harmony::Progression::Kind::body ? juce::String()
                        : entry.kind == harmony::Progression::Kind::cadence ? text("  · Kadenz")
                        : entry.kind == harmony::Progression::Kind::halfCadence ? text("  · Halbschluss") : text("  · Trugschluss");
        g.drawText(entry.name + kind, top, juce::Justification::centredLeft, true);
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(entry.bars()) + text(" Takte"), top, juce::Justification::centredRight);
        g.setColour(Theme::text());
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(harmony::toRomans(entry.degrees, key), area.removeFromTop(17), juce::Justification::centredLeft, true);
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(harmony::toChords(entry.degrees, key), area, juce::Justification::centredLeft, true);
    }
    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { editor.takeFromLibrary(); }

    std::vector<const harmony::Progression*> entries;

private:
    ChordProgressionEditor& editor;
};

ChordProgressionEditor::ChordProgressionEditor(daw::ArrangementController& c, int clickedBar)
    : controller(c), sectionModel(std::make_unique<SectionList>(*this)), libraryModel(std::make_unique<LibraryList>(*this))
{
    sections = controller.state().chordSections;
    const auto title = [this](juce::Label& label, const juce::String& name)
    {
        label.setText(name, juce::dontSendNotification);
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, Theme::subduedText());
        addAndMakeVisible(label);
    };
    title(sectionsTitle, text("ABSCHNITTE"));
    title(libraryTitle, text("BIBLIOTHEK"));
    title(autoTitle, text("AUTO-MODUS"));
    for (auto [label, name] : { std::pair { &nameLabel, "Name" }, std::pair { &fromLabel, "Ab Takt" }, std::pair { &barsLabel, "Takte" },
                                std::pair { &textLabel, "Folge" }, std::pair { &keyLabel, "Grundton" }, std::pair { &modeLabel, "Tonart" },
                                std::pair { &lengthLabel, "Länge" }, std::pair { &autoBarsLabel, "Takte" } })
    {
        label->setText(text(name), juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, Theme::subduedText());
        addAndMakeVisible(*label);
    }
    hint.setText(text("Takte mit | trennen · zwei Akkorde in einem Takt = halbe Takte · % wiederholt den Takt davor · - ist eine Pause. "
                      "Die Folge wird über alle Takte des Abschnitts wiederholt."), juce::dontSendNotification);
    hint.setColour(juce::Label::textColourId, Theme::subduedText().withAlpha(0.8f));
    hint.setFont(juce::FontOptions(11.5f));
    hint.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(hint);
    preview.setColour(juce::Label::textColourId, Theme::amber());
    preview.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(preview);
    autoResult.setColour(juce::Label::textColourId, Theme::amberBright());
    addAndMakeVisible(autoResult);

    sectionBox.setModel(sectionModel.get());
    sectionBox.setRowHeight(26);
    sectionBox.setColour(juce::ListBox::backgroundColourId, Theme::well());
    addAndMakeVisible(sectionBox);
    libraryBox.setModel(libraryModel.get());
    libraryBox.setRowHeight(58);
    libraryBox.setColour(juce::ListBox::backgroundColourId, Theme::well());
    addAndMakeVisible(libraryBox);

    const auto button = [this](juce::TextButton& b, const juce::String& name, std::function<void()> click, bool accent = false)
    {
        b.setButtonText(name);
        b.onClick = std::move(click);
        if (accent)
        {
            b.setColour(juce::TextButton::buttonColourId, Theme::amber());
            b.setColour(juce::TextButton::textColourOffId, Theme::onText());
        }
        addAndMakeVisible(b);
    };
    button(addButton, text("+ Abschnitt"), [this]
    {
        const auto first = sections.empty() ? 0 : sections.back().firstBar + sections.back().bars;
        addSection(first, 4, {});
    });
    button(duplicateButton, text("Duplizieren"), [this]
    {
        if (! juce::isPositiveAndBelow(selected, static_cast<int>(sections.size())))
            return;
        auto copy = sections[static_cast<size_t>(selected)];
        int end = 0;
        for (const auto& section : sections)
            end = std::max(end, section.firstBar + section.bars);
        addSection(end, copy.bars, copy.text);
        sections.back().name = copy.name;
        sections.back().key = copy.key;
        sectionBox.updateContent();
        showSection();
    });
    button(removeButton, text("Löschen"), [this]
    {
        if (! juce::isPositiveAndBelow(selected, static_cast<int>(sections.size())))
            return;
        sections.erase(sections.begin() + selected);
        sectionBox.updateContent();
        selectSection(std::min(selected, static_cast<int>(sections.size()) - 1));
    });
    button(takeButton, text("← In Abschnitt übernehmen"), [this] { takeFromLibrary(); });
    button(autoButton, text("Würfeln"), [this] { runAuto(); });
    button(writeButton, text("In Akkordleiste schreiben"), [this] { writeToLeadSheet(); }, true);
    button(closeButton, text("Schließen"), [this] { if (onClose) onClose(); });
    takeButton.setTooltip(text("Die gewählte Folge in der eingestellten Tonart in den markierten Abschnitt schreiben (auch per Doppelklick)."));
    autoButton.setTooltip(text("Wählt eine Tonart (zufällig oder die eingestellte) und füllt die Takte mit Phrasen aus vier Takten: "
                               "Eröffnung plus Kadenz, Halbschluss in der Mitte, Ganzschluss am Ende, Form A A' B A."));

    for (auto* editor : { &nameEditor, &textEditor })
    {
        editor->setColour(juce::TextEditor::backgroundColourId, Theme::well());
        editor->setColour(juce::TextEditor::textColourId, Theme::text());
        editor->setColour(juce::TextEditor::outlineColourId, Theme::outline());
        editor->setColour(juce::TextEditor::focusedOutlineColourId, Theme::amber());
        editor->onTextChange = [this] { storeSection(); };
        addAndMakeVisible(*editor);
    }
    textEditor.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    for (auto* slider : { &fromSlider, &barsSlider, &autoBars })
    {
        slider->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 52, 22);
        slider->setColour(juce::Slider::textBoxTextColourId, Theme::text());
        addAndMakeVisible(*slider);
    }
    fromSlider.setRange(1, 999, 1);
    barsSlider.setRange(1, 256, 1);
    autoBars.setRange(2, 128, 1);
    fromSlider.onValueChange = [this] { storeSection(); };
    barsSlider.onValueChange = [this] { storeSection(); };
    autoBars.setValue(16, juce::dontSendNotification);

    for (int pitch = 0; pitch < 12; ++pitch)
    {
        static const char* names[] { "C", "C# / Db", "D", "D# / Eb", "E", "F", "F# / Gb", "G", "G# / Ab", "A", "A# / B", "H" };
        tonicBox.addItem(text(names[pitch]), pitch + 1);
    }
    for (int mode = 0; mode < harmony::numModes; ++mode)
        modeBox.addItem(harmony::modeName(static_cast<harmony::Mode>(mode)), mode + 1);
    lengthBox.addItemList({ text("Alle"), text("2 Takte"), text("4 Takte"), text("8 Takte"), text("12 Takte"), text("16 Takte"), text("Nur Kadenzen") }, 1);
    tonicBox.setSelectedId(10, juce::dontSendNotification);
    modeBox.setSelectedId(static_cast<int>(harmony::Mode::aeolian) + 1, juce::dontSendNotification);
    lengthBox.setSelectedId(1, juce::dontSendNotification);
    for (auto* box : { &tonicBox, &modeBox, &lengthBox })
    {
        box->onChange = [this] { refreshLibrary(); };
        addAndMakeVisible(*box);
    }
    randomKey.setButtonText(text("Tonart zufällig"));
    randomKey.setToggleState(true, juce::dontSendNotification);
    randomKey.setColour(juce::ToggleButton::textColourId, Theme::text());
    randomKey.setColour(juce::ToggleButton::tickColourId, Theme::amber());
    randomKey.setTooltip(text("Aus: der Auto-Modus nimmt die oben eingestellte Tonart."));
    addAndMakeVisible(randomKey);

    // Start at the clicked bar: its section, or a new one there.
    int found = -1;
    for (size_t index = 0; index < sections.size(); ++index)
        if (clickedBar >= sections[index].firstBar && clickedBar < sections[index].firstBar + sections[index].bars)
            found = static_cast<int>(index);
    refreshLibrary();
    if (found >= 0)
    {
        sectionBox.updateContent();
        sectionBox.selectRow(found);
    }
    else
        addSection(std::max(0, clickedBar), 4, {});
    setSize(1080, 680);
}

ChordProgressionEditor::~ChordProgressionEditor()
{
    sectionBox.setModel(nullptr);
    libraryBox.setModel(nullptr);
}

harmony::Key ChordProgressionEditor::key() const
{
    return { std::max(0, tonicBox.getSelectedId() - 1), static_cast<harmony::Mode>(std::max(0, modeBox.getSelectedId() - 1)) };
}

std::vector<const harmony::Progression*> ChordProgressionEditor::filteredLibrary() const
{
    std::vector<const harmony::Progression*> list;
    const auto choice = lengthBox.getSelectedId();
    static const int lengths[] { 0, 2, 4, 8, 12, 16 };
    for (const auto& entry : harmony::library(key().mode))
    {
        const auto isCadence = entry.kind != harmony::Progression::Kind::body;
        if (choice == 7 ? isCadence : (choice <= 1 || (! isCadence && entry.bars() == lengths[choice - 1])))
            list.push_back(&entry);
    }
    return list;
}

void ChordProgressionEditor::refreshLibrary()
{
    libraryModel->entries = filteredLibrary();
    libraryBox.updateContent();
    libraryBox.repaint();
    if (libraryBox.getSelectedRow() < 0 && ! libraryModel->entries.empty())
        libraryBox.selectRow(0);
}

void ChordProgressionEditor::addSection(int firstBar, int bars, const juce::String& progression)
{
    daw::ChordSection section;
    section.name = text("Teil ") + juce::String(static_cast<int>(sections.size()) + 1);
    section.firstBar = firstBar;
    section.bars = bars;
    section.text = progression;
    sections.push_back(section);
    sectionBox.updateContent();
    sectionBox.selectRow(static_cast<int>(sections.size()) - 1);
    selectSection(static_cast<int>(sections.size()) - 1);
}

void ChordProgressionEditor::selectSection(int index)
{
    selected = juce::isPositiveAndBelow(index, static_cast<int>(sections.size())) ? index : -1;
    showSection();
}

void ChordProgressionEditor::showSection()
{
    const juce::ScopedValueSetter<bool> guard(updating, true);
    const auto valid = selected >= 0;
    for (auto* component : std::initializer_list<juce::Component*> { &nameEditor, &textEditor, &fromSlider, &barsSlider, &duplicateButton, &removeButton })
        component->setEnabled(valid);
    if (! valid)
    {
        nameEditor.clear();
        textEditor.clear();
        preview.setText({}, juce::dontSendNotification);
        return;
    }
    const auto& section = sections[static_cast<size_t>(selected)];
    nameEditor.setText(section.name, false);
    textEditor.setText(section.text, false);
    fromSlider.setValue(section.firstBar + 1, juce::dontSendNotification);
    barsSlider.setValue(section.bars, juce::dontSendNotification);
    preview.setText(section.key.isNotEmpty() ? text("Tonart: ") + section.key : juce::String(), juce::dontSendNotification);
}

void ChordProgressionEditor::storeSection()
{
    if (updating || selected < 0)
        return;
    auto& section = sections[static_cast<size_t>(selected)];
    section.name = nameEditor.getText().trim();
    section.text = textEditor.getText().trim();
    section.firstBar = static_cast<int>(fromSlider.getValue()) - 1;
    section.bars = static_cast<int>(barsSlider.getValue());
    // Names the lead sheet would show in red are pointed out right away.
    juce::StringArray unknown;
    for (const auto& bar : harmony::parseBars(section.text))
        for (const auto& name : bar)
            if (! daw::edit::isChordName(daw::edit::tidyChordName(name)))
                unknown.addIfNotAlreadyThere(name);
    preview.setText(unknown.isEmpty() ? (section.key.isNotEmpty() ? text("Tonart: ") + section.key : juce::String())
                                      : text("Unbekannt: ") + unknown.joinIntoString(", "), juce::dontSendNotification);
    preview.setColour(juce::Label::textColourId, unknown.isEmpty() ? Theme::amber() : Theme::danger());
    sectionBox.repaintRow(selected);
}

void ChordProgressionEditor::takeFromLibrary()
{
    const auto row = libraryBox.getSelectedRow();
    if (! juce::isPositiveAndBelow(row, static_cast<int>(libraryModel->entries.size())))
        return;
    const auto& entry = *libraryModel->entries[static_cast<size_t>(row)];
    if (selected < 0)
    {
        const auto first = sections.empty() ? 0 : sections.back().firstBar + sections.back().bars;
        addSection(first, entry.bars(), {});
    }
    auto& section = sections[static_cast<size_t>(selected)];
    section.text = harmony::toChords(entry.degrees, key());
    section.key = harmony::keyName(key());
    if (section.name.startsWith(text("Teil ")))
        section.name = entry.name;
    // An empty or shorter section grows to the progression; a longer one repeats it.
    if (section.bars < entry.bars())
        section.bars = entry.bars();
    sectionBox.updateContent();
    sectionBox.repaintRow(selected);
    showSection();
}

void ChordProgressionEditor::runAuto()
{
    const auto first = selected >= 0 ? sections[static_cast<size_t>(selected)].firstBar : 0;
    const auto bars = static_cast<int>(autoBars.getValue());
    const auto result = harmony::autoFill(first, bars, random, randomKey.getToggleState() ? std::nullopt : std::optional<harmony::Key>(key()));
    // The generated sections replace those inside the range.
    sections.erase(std::remove_if(sections.begin(), sections.end(), [&](const daw::ChordSection& s) { return s.firstBar < first + bars && s.firstBar + s.bars > first; }),
                   sections.end());
    sections.insert(sections.end(), result.sections.begin(), result.sections.end());
    std::stable_sort(sections.begin(), sections.end(), [](const daw::ChordSection& a, const daw::ChordSection& b) { return a.firstBar < b.firstBar; });
    {
        const juce::ScopedValueSetter<bool> guard(updating, true);
        tonicBox.setSelectedId(result.key.tonic + 1, juce::dontSendNotification);
        modeBox.setSelectedId(static_cast<int>(result.key.mode) + 1, juce::dontSendNotification);
    }
    refreshLibrary();
    autoResult.setText(text("Gewürfelt: ") + harmony::keyName(result.key) + text(" · ") + juce::String(bars) + text(" Takte ab Takt ") + juce::String(first + 1),
                       juce::dontSendNotification);
    sectionBox.updateContent();
    for (size_t index = 0; index < sections.size(); ++index)
        if (sections[index].firstBar == first)
        {
            sectionBox.selectRow(static_cast<int>(index));
            selectSection(static_cast<int>(index)); // also when that row was selected before
            break;
        }
    repaint();
}

void ChordProgressionEditor::writeToLeadSheet()
{
    const auto copy = sections;
    controller.perform(text("Akkordfolgen"), [&copy](daw::ArrangementState& state)
    {
        state.chordSections = copy;
        for (const auto& section : copy)
            harmony::apply(state, section);
    });
    if (onClose)
        onClose();
}

void ChordProgressionEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::surface());
    g.setColour(Theme::outline());
    g.drawVerticalLine(getWidth() / 2 - 4, 12.0f, static_cast<float>(getHeight() - 110));
    g.drawHorizontalLine(getHeight() - 104, 12.0f, static_cast<float>(getWidth() - 12));
}

void ChordProgressionEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto bottom = area.removeFromBottom(88);
    area.removeFromBottom(12);
    auto left = area.removeFromLeft(area.getWidth() / 2 - 12);
    area.removeFromLeft(24);
    auto right = area;

    sectionsTitle.setBounds(left.removeFromTop(22));
    auto details = left.removeFromBottom(212);
    auto buttons = left.removeFromBottom(30);
    left.removeFromBottom(6);
    sectionBox.setBounds(left);
    addButton.setBounds(buttons.removeFromLeft(110));
    buttons.removeFromLeft(6);
    duplicateButton.setBounds(buttons.removeFromLeft(110));
    buttons.removeFromLeft(6);
    removeButton.setBounds(buttons.removeFromLeft(90));
    details.removeFromTop(10);
    auto row = details.removeFromTop(26);
    nameLabel.setBounds(row.removeFromLeft(60));
    barsSlider.setBounds(row.removeFromRight(110));
    barsLabel.setBounds(row.removeFromRight(44));
    row.removeFromRight(8);
    fromSlider.setBounds(row.removeFromRight(110));
    fromLabel.setBounds(row.removeFromRight(56));
    row.removeFromRight(10);
    nameEditor.setBounds(row);
    details.removeFromTop(8);
    row = details.removeFromTop(34);
    textLabel.setBounds(row.removeFromLeft(60));
    textEditor.setBounds(row);
    details.removeFromTop(4);
    preview.setBounds(details.removeFromTop(20).withTrimmedLeft(60));
    hint.setBounds(details.removeFromTop(48).withTrimmedLeft(60));

    libraryTitle.setBounds(right.removeFromTop(22));
    row = right.removeFromTop(26);
    keyLabel.setBounds(row.removeFromLeft(66));
    tonicBox.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(10);
    modeLabel.setBounds(row.removeFromLeft(50));
    modeBox.setBounds(row.removeFromLeft(150));
    row.removeFromLeft(10);
    lengthLabel.setBounds(row.removeFromLeft(48));
    lengthBox.setBounds(row);
    right.removeFromTop(8);
    takeButton.setBounds(right.removeFromBottom(30).removeFromLeft(220));
    right.removeFromBottom(6);
    libraryBox.setBounds(right);

    bottom.removeFromTop(6);
    autoTitle.setBounds(bottom.removeFromTop(22));
    row = bottom.removeFromTop(30);
    autoBarsLabel.setBounds(row.removeFromLeft(48));
    autoBars.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(12);
    randomKey.setBounds(row.removeFromLeft(150));
    autoButton.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(12);
    closeButton.setBounds(row.removeFromRight(110));
    row.removeFromRight(8);
    writeButton.setBounds(row.removeFromRight(210));
    autoResult.setBounds(row);
    hint.toFront(false);
}
} // namespace nam::ui
