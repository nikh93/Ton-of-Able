#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"
#include "daw/Harmony.h"

namespace nam::ui
{
/** The chord progression editor behind the lead sheet: sections (a
    progression over a range of bars, repeated), a library of progressions
    per key in all church modes and harmonic/melodic minor (shown as scale
    degrees and chords), and an auto mode that picks a key and fills the bars
    with phrases and cadences. "In Akkordleiste schreiben" applies everything
    as one undo step. */
class ChordProgressionEditor final : public juce::Component
{
public:
    ChordProgressionEditor(daw::ArrangementController&, int clickedBar);
    ~ChordProgressionEditor() override;

    std::function<void()> onClose;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class SectionList;
    class LibraryList;

    [[nodiscard]] daw::harmony::Key key() const;
    [[nodiscard]] std::vector<const daw::harmony::Progression*> filteredLibrary() const;
    void selectSection(int index);
    void showSection();
    void storeSection();
    void addSection(int firstBar, int bars, const juce::String& text);
    void takeFromLibrary();
    void runAuto();
    void writeToLeadSheet();
    void refreshLibrary();

    daw::ArrangementController& controller;
    std::vector<daw::ChordSection> sections;
    int selected = -1;
    bool updating = false;

    juce::Label sectionsTitle, libraryTitle, nameLabel, fromLabel, barsLabel, textLabel, hint, keyLabel, modeLabel, lengthLabel,
        autoTitle, autoBarsLabel, autoResult, preview;
    std::unique_ptr<SectionList> sectionModel;
    std::unique_ptr<LibraryList> libraryModel;
    juce::ListBox sectionBox, libraryBox;
    juce::TextButton addButton, duplicateButton, removeButton, takeButton, autoButton, writeButton, closeButton;
    juce::TextEditor nameEditor, textEditor;
    juce::Slider fromSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 barsSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 autoBars { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::ComboBox tonicBox, modeBox, lengthBox, autoTonic, autoMode;
    juce::Label autoKeyLabel;
    juce::Random random;
};
} // namespace nam::ui
