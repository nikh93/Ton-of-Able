#include "DrumArrangementEditor.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using daw::text;
using Theme = AmberTheme;
namespace grooves = daw::grooves;

namespace
{
/** A pattern as a small step grid: one row per used lane, accents brighter. */
void paintGrid(juce::Graphics& g, const daw::DrumPattern& pattern, juce::Rectangle<float> area)
{
    std::vector<size_t> lanes;
    for (size_t lane = 0; lane < pattern.velocity.size(); ++lane)
        for (int step = 0; step < pattern.steps; ++step)
            if (pattern.velocity[lane][static_cast<size_t>(step)] > 0)
            {
                lanes.push_back(lane);
                break;
            }
    if (lanes.empty() || pattern.steps <= 0)
        return;
    const auto cellWidth = area.getWidth() / static_cast<float>(pattern.steps);
    const auto cellHeight = std::min(7.0f, area.getHeight() / static_cast<float>(lanes.size()));
    for (size_t row = 0; row < lanes.size(); ++row)
        for (int step = 0; step < pattern.steps; ++step)
        {
            const auto cell = juce::Rectangle<float>(area.getX() + step * cellWidth, area.getY() + row * cellHeight, cellWidth, cellHeight).reduced(0.7f);
            const auto velocity = pattern.velocity[lanes[row]][static_cast<size_t>(step)];
            g.setColour(velocity == 0 ? juce::Colours::white.withAlpha(step % 4 == 0 ? 0.09f : 0.04f)
                                      : Theme::amber().withAlpha(0.35f + 0.65f * static_cast<float>(velocity) / 127.0f));
            g.fillRect(cell);
        }
}

juce::String everyText(int every)
{
    return every < 0 ? text("kein Fill") : every == 0 ? text("Fill am Ende") : text("Fill alle ") + juce::String(every);
}
} // namespace

class DrumArrangementEditor::SectionList final : public juce::ListBoxModel
{
public:
    explicit SectionList(DrumArrangementEditor& e) : editor(e) {}
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
        g.drawText(section.name, area.removeFromLeft(100), juce::Justification::centredLeft, true);
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(text("Takt ") + juce::String(section.firstBar + 1) + text("–") + juce::String(section.firstBar + section.bars),
                   area.removeFromLeft(86), juce::Justification::centredLeft, true);
        g.setColour(Theme::text());
        auto summary = section.groove.isEmpty() ? text("(Pause)") : section.groove;
        if (section.fill.isNotEmpty() && section.fillEvery >= 0)
            summary << text(" · ") << everyText(section.fillEvery);
        if (section.crash)
            summary << text(" · Crash");
        g.drawText(summary, area, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged(int row) override { editor.selectSection(row); }

private:
    DrumArrangementEditor& editor;
};

class DrumArrangementEditor::LibraryList final : public juce::ListBoxModel
{
public:
    explicit LibraryList(DrumArrangementEditor& e) : editor(e) {}
    int getNumRows() override { return static_cast<int>(entries.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow(row, getNumRows()))
            return;
        const auto& entry = *entries[static_cast<size_t>(row)];
        g.fillAll(selected ? Theme::amber().withAlpha(0.25f) : (row % 2 == 0 ? Theme::well() : Theme::panel()));
        auto area = juce::Rectangle<int>(0, 0, width, height).reduced(8, 4);
        auto top = area.removeFromTop(18);
        g.setColour(Theme::amberBright());
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(entry.pattern.name + (entry.fill ? text("  · Fill") : juce::String()), top, juce::Justification::centredLeft, true);
        g.setColour(Theme::subduedText());
        g.setFont(juce::FontOptions(11.0f));
        const auto [low, high] = grooves::tempoRange(entry.genre);
        g.drawText(juce::String(low) + text("–") + juce::String(high) + text(" BPM") + (entry.pattern.swing > 0.0f ? text(" · Swing") : juce::String()),
                   top, juce::Justification::centredRight);
        area.removeFromTop(3);
        paintGrid(g, entry.pattern, area.toFloat());
    }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow(row, getNumRows()))
            editor.takeFromLibrary(entries[static_cast<size_t>(row)]->fill);
    }

    std::vector<const grooves::Groove*> entries;

private:
    DrumArrangementEditor& editor;
};

DrumArrangementEditor::DrumArrangementEditor(daw::ArrangementController& c, const juce::Uuid& id, int clickedBar)
    : controller(c), trackId(id), sectionModel(std::make_unique<SectionList>(*this)), libraryModel(std::make_unique<LibraryList>(*this))
{
    if (const auto* track = controller.state().findTrack(trackId))
        sections = track->drumSections;
    const auto title = [this](juce::Label& label, const juce::String& name)
    {
        label.setText(name, juce::dontSendNotification);
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, Theme::subduedText());
        addAndMakeVisible(label);
    };
    const auto* track = controller.state().findTrack(trackId);
    title(sectionsTitle, text("ABSCHNITTE · ") + (track != nullptr ? track->name.toUpperCase() : juce::String()));
    title(libraryTitle, text("GROOVE-BIBLIOTHEK"));
    title(autoTitle, text("AUTO-MODUS"));
    for (auto [label, name] : { std::pair { &nameLabel, "Name" }, std::pair { &fromLabel, "Ab Takt" }, std::pair { &barsLabel, "Takte" },
                                std::pair { &grooveLabel, "Groove" }, std::pair { &fillLabel, "Fill" }, std::pair { &everyLabel, "Wann" },
                                std::pair { &styleLabel, "Stil" }, std::pair { &kindLabel, "Zeige" }, std::pair { &autoStyleLabel, "Stil" },
                                std::pair { &autoBarsLabel, "Takte" } })
    {
        label->setText(text(name), juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, Theme::subduedText());
        addAndMakeVisible(*label);
    }
    info.setColour(juce::Label::textColourId, Theme::subduedText().withAlpha(0.85f));
    info.setFont(juce::FontOptions(12.0f));
    info.setMinimumHorizontalScale(1.0f);
    info.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(info);
    autoResult.setColour(juce::Label::textColourId, Theme::amberBright());
    addAndMakeVisible(autoResult);

    sectionBox.setModel(sectionModel.get());
    sectionBox.setRowHeight(26);
    sectionBox.setColour(juce::ListBox::backgroundColourId, Theme::well());
    addAndMakeVisible(sectionBox);
    libraryBox.setModel(libraryModel.get());
    libraryBox.setRowHeight(64);
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
        int end = 0;
        for (const auto& section : sections)
            end = std::max(end, section.firstBar + section.bars);
        addSection(end, 8);
    });
    button(duplicateButton, text("Duplizieren"), [this]
    {
        if (selected < 0)
            return;
        auto copy = sections[static_cast<size_t>(selected)];
        int end = 0;
        for (const auto& section : sections)
            end = std::max(end, section.firstBar + section.bars);
        copy.firstBar = end;
        sections.push_back(copy);
        sectionBox.updateContent();
        sectionBox.selectRow(static_cast<int>(sections.size()) - 1);
        selectSection(static_cast<int>(sections.size()) - 1);
    });
    button(removeButton, text("Löschen"), [this]
    {
        if (selected < 0)
            return;
        sections.erase(sections.begin() + selected);
        sectionBox.updateContent();
        selectSection(std::min(selected, static_cast<int>(sections.size()) - 1));
    });
    button(asGroove, text("← Als Groove"), [this] { takeFromLibrary(false); });
    button(asFill, text("← Als Fill"), [this] { takeFromLibrary(true); });
    button(autoButton, text("Würfeln"), [this] { runAuto(); });
    button(writeButton, text("In Drum-Spur schreiben"), [this] { write(); }, true);
    button(closeButton, text("Schließen"), [this] { if (onClose) onClose(); });
    asGroove.setTooltip(text("Den gewählten Groove für den markierten Abschnitt verwenden (Doppelklick: je nach Art Groove oder Fill)."));
    asFill.setTooltip(text("Das gewählte Pattern als Fill des markierten Abschnitts verwenden."));
    autoButton.setTooltip(text("Wählt einen Stil (fest oder zufällig) und arrangiert die Takte: Strophe, Strophe, Refrain, Refrain … "
                               "mit Fills am Phrasenende, Crash zu Beginn neuer Teile und einem Schlusstakt."));

    nameEditor.setColour(juce::TextEditor::backgroundColourId, Theme::well());
    nameEditor.setColour(juce::TextEditor::textColourId, Theme::text());
    nameEditor.setColour(juce::TextEditor::outlineColourId, Theme::outline());
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::amber());
    nameEditor.onTextChange = [this] { storeSection(); };
    addAndMakeVisible(nameEditor);
    for (auto* slider : { &fromSlider, &barsSlider, &autoBars })
    {
        slider->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 52, 22);
        slider->setColour(juce::Slider::textBoxTextColourId, Theme::text());
        addAndMakeVisible(*slider);
    }
    fromSlider.setRange(1, 999, 1);
    barsSlider.setRange(1, 256, 1);
    autoBars.setRange(2, 256, 1);
    autoBars.setValue(32, juce::dontSendNotification);
    fromSlider.onValueChange = [this] { storeSection(); };
    barsSlider.onValueChange = [this] { storeSection(); };

    everyBox.addItem(text("Kein Fill"), 1);
    everyBox.addItem(text("Nur im letzten Takt"), 2);
    everyBox.addItem(text("Alle 2 Takte"), 3);
    everyBox.addItem(text("Alle 4 Takte"), 4);
    everyBox.addItem(text("Alle 8 Takte"), 5);
    for (auto* box : { &grooveBox, &fillBox, &everyBox })
    {
        box->onChange = [this] { storeSection(); };
        addAndMakeVisible(*box);
    }
    crashToggle.setButtonText(text("Crash am Anfang"));
    crashToggle.onClick = [this] { storeSection(); };
    tempoToggle.setButtonText(text("Tempo passend setzen"));
    tempoToggle.setTooltip(text("Beim Schreiben das Song-Tempo auf ein typisches Tempo des gewürfelten Stils setzen."));
    for (auto* toggle : { &crashToggle, &tempoToggle })
    {
        toggle->setColour(juce::ToggleButton::textColourId, Theme::text());
        toggle->setColour(juce::ToggleButton::tickColourId, Theme::amber());
        addAndMakeVisible(*toggle);
    }

    styleBox.addItem(text("Alle Stile"), 100);
    autoStyle.addItem(text("Zufällig"), 100);
    for (int index = 0; index < grooves::genres().size(); ++index)
    {
        styleBox.addItem(grooves::genres()[index], index + 1);
        autoStyle.addItem(grooves::genres()[index], index + 1);
    }
    styleBox.setSelectedId(1, juce::dontSendNotification);
    autoStyle.setSelectedId(100, juce::dontSendNotification);
    kindBox.addItemList({ text("Grooves"), text("Fills"), text("Grooves und Fills") }, 1);
    kindBox.setSelectedId(3, juce::dontSendNotification);
    for (auto* box : { &styleBox, &kindBox })
    {
        box->onChange = [this] { refreshLibrary(); };
        addAndMakeVisible(*box);
    }
    addAndMakeVisible(autoStyle);

    const auto steps = grooves::stepsPerBar(controller.state());
    info.setText(text("Taktart ") + juce::String(controller.state().beatsPerBar) + "/" + juce::String(controller.state().beatUnit) + text(": die Bibliothek zeigt passende Patterns (")
                 + juce::String(steps) + text(" Sechzehntel). Bearbeiten lassen sich die Patterns im Drumcomputer der Spur."), juce::dontSendNotification);

    refreshLibrary();
    refreshPatternChoices();
    int found = -1;
    for (size_t index = 0; index < sections.size(); ++index)
        if (clickedBar >= sections[index].firstBar && clickedBar < sections[index].firstBar + sections[index].bars)
            found = static_cast<int>(index);
    if (found >= 0)
    {
        sectionBox.updateContent();
        sectionBox.selectRow(found);
        selectSection(found);
    }
    else
        addSection(std::max(0, clickedBar), 8);
    setSize(1100, 700);
}

DrumArrangementEditor::~DrumArrangementEditor()
{
    sectionBox.setModel(nullptr);
    libraryBox.setModel(nullptr);
}

juce::StringArray DrumArrangementEditor::patternNames() const
{
    juce::StringArray names;
    if (const auto* track = controller.state().findTrack(trackId))
        for (const auto& pattern : track->patterns)
            if (! pattern.name.endsWith(" + Crash"))
                names.addIfNotAlreadyThere(pattern.name);
    for (const auto& pattern : added)
        names.addIfNotAlreadyThere(pattern.name);
    return names;
}

const daw::DrumPattern* DrumArrangementEditor::patternNamed(const juce::String& name) const
{
    for (const auto& pattern : added)
        if (pattern.name == name)
            return &pattern;
    if (const auto* track = controller.state().findTrack(trackId))
        for (const auto& pattern : track->patterns)
            if (pattern.name == name)
                return &pattern;
    return nullptr;
}

void DrumArrangementEditor::refreshPatternChoices()
{
    const juce::ScopedValueSetter<bool> guard(updating, true);
    const auto names = patternNames();
    grooveBox.clear(juce::dontSendNotification);
    fillBox.clear(juce::dontSendNotification);
    grooveBox.addItem(text("(Pause – keine Drums)"), 1000);
    fillBox.addItem(text("(kein Fill)"), 1000);
    for (int index = 0; index < names.size(); ++index)
    {
        grooveBox.addItem(names[index], index + 1);
        fillBox.addItem(names[index], index + 1);
    }
    showSection();
}

void DrumArrangementEditor::refreshLibrary()
{
    const auto steps = grooves::stepsPerBar(controller.state());
    const auto genre = styleBox.getSelectedId() == 100 ? juce::String() : grooves::genres()[styleBox.getSelectedId() - 1];
    libraryModel->entries.clear();
    const auto kind = kindBox.getSelectedId();
    if (kind != 2)
        for (const auto* entry : grooves::find(genre, steps, false))
            libraryModel->entries.push_back(entry);
    if (kind != 1)
        for (const auto* entry : grooves::find(genre, steps, true))
            libraryModel->entries.push_back(entry);
    libraryBox.updateContent();
    libraryBox.repaint();
    if (! libraryModel->entries.empty())
        libraryBox.selectRow(0);
}

void DrumArrangementEditor::addSection(int firstBar, int bars)
{
    daw::DrumSection section;
    section.name = text("Teil ") + juce::String(static_cast<int>(sections.size()) + 1);
    section.firstBar = firstBar;
    section.bars = bars;
    if (! sections.empty())
    {
        section.groove = sections.back().groove;
        section.fill = sections.back().fill;
    }
    sections.push_back(section);
    sectionBox.updateContent();
    sectionBox.selectRow(static_cast<int>(sections.size()) - 1);
    selectSection(static_cast<int>(sections.size()) - 1);
}

void DrumArrangementEditor::selectSection(int index)
{
    selected = juce::isPositiveAndBelow(index, static_cast<int>(sections.size())) ? index : -1;
    showSection();
}

void DrumArrangementEditor::showSection()
{
    const juce::ScopedValueSetter<bool> guard(updating, true);
    const auto valid = selected >= 0;
    for (auto* component : std::initializer_list<juce::Component*> { &nameEditor, &fromSlider, &barsSlider, &grooveBox, &fillBox, &everyBox,
                                                                     &crashToggle, &duplicateButton, &removeButton })
        component->setEnabled(valid);
    if (! valid)
        return;
    const auto& section = sections[static_cast<size_t>(selected)];
    nameEditor.setText(section.name, false);
    fromSlider.setValue(section.firstBar + 1, juce::dontSendNotification);
    barsSlider.setValue(section.bars, juce::dontSendNotification);
    const auto names = patternNames();
    grooveBox.setSelectedId(section.groove.isEmpty() ? 1000 : names.indexOf(section.groove) + 1, juce::dontSendNotification);
    fillBox.setSelectedId(section.fill.isEmpty() ? 1000 : names.indexOf(section.fill) + 1, juce::dontSendNotification);
    everyBox.setSelectedId(section.fillEvery < 0 ? 1 : section.fillEvery == 0 ? 2 : section.fillEvery <= 2 ? 3 : section.fillEvery <= 4 ? 4 : 5,
                           juce::dontSendNotification);
    crashToggle.setToggleState(section.crash, juce::dontSendNotification);
}

void DrumArrangementEditor::storeSection()
{
    if (updating || selected < 0)
        return;
    auto& section = sections[static_cast<size_t>(selected)];
    section.name = nameEditor.getText().trim();
    section.firstBar = static_cast<int>(fromSlider.getValue()) - 1;
    section.bars = static_cast<int>(barsSlider.getValue());
    const auto names = patternNames();
    section.groove = grooveBox.getSelectedId() == 1000 ? juce::String() : names[grooveBox.getSelectedId() - 1];
    section.fill = fillBox.getSelectedId() == 1000 ? juce::String() : names[fillBox.getSelectedId() - 1];
    static const int every[] { -1, 0, 2, 4, 8 };
    section.fillEvery = every[juce::jlimit(1, 5, everyBox.getSelectedId()) - 1];
    section.crash = crashToggle.getToggleState();
    sectionBox.repaintRow(selected);
}

void DrumArrangementEditor::takeFromLibrary(bool asFillPattern)
{
    const auto row = libraryBox.getSelectedRow();
    if (! juce::isPositiveAndBelow(row, static_cast<int>(libraryModel->entries.size())))
        return;
    const auto& pattern = libraryModel->entries[static_cast<size_t>(row)]->pattern;
    if (patternNamed(pattern.name) == nullptr)
        added.push_back(pattern);
    if (selected < 0)
        addSection(0, 8);
    auto& section = sections[static_cast<size_t>(selected)];
    if (asFillPattern)
    {
        section.fill = pattern.name;
        if (section.fillEvery < 0)
            section.fillEvery = 0;
    }
    else
    {
        section.groove = pattern.name;
        if (section.name.startsWith(text("Teil ")))
            section.name = pattern.name.fromFirstOccurrenceOf(": ", false, false);
    }
    refreshPatternChoices();
    sectionBox.repaintRow(selected);
}

void DrumArrangementEditor::runAuto()
{
    const auto first = selected >= 0 ? sections[static_cast<size_t>(selected)].firstBar : 0;
    const auto bars = static_cast<int>(autoBars.getValue());
    const auto wanted = autoStyle.getSelectedId() == 100 ? juce::String() : grooves::genres()[autoStyle.getSelectedId() - 1];
    const auto result = grooves::autoFill(first, bars, grooves::stepsPerBar(controller.state()), random, wanted);
    if (result.sections.empty())
    {
        autoResult.setText(text("Für diese Taktart gibt es keine passenden Grooves."), juce::dontSendNotification);
        return;
    }
    for (const auto& pattern : result.patterns)
        if (patternNamed(pattern.name) == nullptr)
            added.push_back(pattern);
    sections.erase(std::remove_if(sections.begin(), sections.end(), [&](const daw::DrumSection& s) { return s.firstBar < first + bars && s.firstBar + s.bars > first; }),
                   sections.end());
    sections.insert(sections.end(), result.sections.begin(), result.sections.end());
    std::stable_sort(sections.begin(), sections.end(), [](const daw::DrumSection& a, const daw::DrumSection& b) { return a.firstBar < b.firstBar; });
    suggestedBpm = result.bpm;
    autoResult.setText(text("Gewürfelt: ") + result.genre + text(" · ") + juce::String(bars) + text(" Takte ab Takt ") + juce::String(first + 1)
                       + text(" · passendes Tempo ") + juce::String(static_cast<int>(result.bpm)) + text(" BPM"), juce::dontSendNotification);
    {
        const juce::ScopedValueSetter<bool> guard(updating, true);
        styleBox.setSelectedId(grooves::genres().indexOf(result.genre) + 1, juce::dontSendNotification);
    }
    refreshLibrary();
    refreshPatternChoices();
    sectionBox.updateContent();
    for (size_t index = 0; index < sections.size(); ++index)
        if (sections[index].firstBar == first)
        {
            sectionBox.selectRow(static_cast<int>(index));
            selectSection(static_cast<int>(index));
            break;
        }
    repaint();
}

void DrumArrangementEditor::write()
{
    const auto copy = sections;
    const auto patterns = added;
    const auto bpm = tempoToggle.getToggleState() ? suggestedBpm : 0.0;
    const auto id = trackId;
    controller.perform(text("Drum-Arrangement"), [&](daw::ArrangementState& state)
    {
        const auto index = state.trackIndex(id);
        if (index < 0)
            return;
        if (bpm > 0.0)
            state.bpm = bpm;
        auto& track = state.tracks[static_cast<size_t>(index)];
        for (const auto& pattern : patterns)
            grooves::ensurePattern(track, pattern);
        track.drumSections = copy;
        for (const auto& section : copy)
            grooves::apply(state, index, section);
    });
    if (onClose)
        onClose();
}

void DrumArrangementEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::surface());
    g.setColour(Theme::outline());
    g.drawVerticalLine(getWidth() / 2 - 4, 12.0f, static_cast<float>(getHeight() - 116));
    g.drawHorizontalLine(getHeight() - 110, 12.0f, static_cast<float>(getWidth() - 12));
}

void DrumArrangementEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto bottom = area.removeFromBottom(94);
    area.removeFromBottom(12);
    auto left = area.removeFromLeft(area.getWidth() / 2 - 12);
    area.removeFromLeft(24);
    auto right = area;

    sectionsTitle.setBounds(left.removeFromTop(22));
    auto details = left.removeFromBottom(196);
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
    row = details.removeFromTop(26);
    grooveLabel.setBounds(row.removeFromLeft(60));
    grooveBox.setBounds(row);
    details.removeFromTop(6);
    row = details.removeFromTop(26);
    fillLabel.setBounds(row.removeFromLeft(60));
    everyBox.setBounds(row.removeFromRight(170));
    everyLabel.setBounds(row.removeFromRight(44));
    row.removeFromRight(8);
    fillBox.setBounds(row);
    details.removeFromTop(6);
    crashToggle.setBounds(details.removeFromTop(26).withTrimmedLeft(56).removeFromLeft(200));
    info.setBounds(details.withTrimmedLeft(60));

    libraryTitle.setBounds(right.removeFromTop(22));
    row = right.removeFromTop(26);
    styleLabel.setBounds(row.removeFromLeft(40));
    styleBox.setBounds(row.removeFromLeft(190));
    row.removeFromLeft(12);
    kindLabel.setBounds(row.removeFromLeft(46));
    kindBox.setBounds(row.removeFromLeft(170));
    right.removeFromTop(8);
    auto take = right.removeFromBottom(30);
    asGroove.setBounds(take.removeFromLeft(130));
    take.removeFromLeft(8);
    asFill.setBounds(take.removeFromLeft(110));
    right.removeFromBottom(6);
    libraryBox.setBounds(right);

    bottom.removeFromTop(6);
    autoTitle.setBounds(bottom.removeFromTop(22));
    row = bottom.removeFromTop(30);
    autoStyleLabel.setBounds(row.removeFromLeft(36));
    autoStyle.setBounds(row.removeFromLeft(160).reduced(0, 2));
    row.removeFromLeft(12);
    autoBarsLabel.setBounds(row.removeFromLeft(44));
    autoBars.setBounds(row.removeFromLeft(110));
    row.removeFromLeft(12);
    autoButton.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(12);
    tempoToggle.setBounds(row.removeFromLeft(190));
    closeButton.setBounds(row.removeFromRight(100));
    row.removeFromRight(8);
    writeButton.setBounds(row.removeFromRight(200));
    autoResult.setBounds(bottom.removeFromTop(24).withTrimmedLeft(36));
}
} // namespace nam::ui
