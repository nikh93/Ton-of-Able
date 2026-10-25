#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "daw/ArrangementController.h"
#include "daw/DrumStyles.h"

namespace nam::ui
{
/** The drum counterpart of the chord progression editor, for one drum track:
    sections (a groove over a range of bars with fills and a crash), a library
    of grooves and fills by style shown as step grids, and an auto mode that
    picks a style (or the chosen one) and arranges verses, choruses, fills and
    an ending. "In Drum-Spur schreiben" applies everything as one undo step. */
class DrumArrangementEditor final : public juce::Component
{
public:
    DrumArrangementEditor(daw::ArrangementController&, const juce::Uuid& trackId, int clickedBar);
    ~DrumArrangementEditor() override;

    std::function<void()> onClose;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class SectionList;
    class LibraryList;

    [[nodiscard]] juce::StringArray patternNames() const;
    [[nodiscard]] const daw::DrumPattern* patternNamed(const juce::String&) const;
    void selectSection(int index);
    void showSection();
    void storeSection();
    void addSection(int firstBar, int bars);
    void refreshPatternChoices();
    void refreshLibrary();
    void takeFromLibrary(bool asFill);
    void runAuto();
    void updateFollow();
    void write();

    daw::ArrangementController& controller;
    juce::Uuid trackId;
    std::vector<daw::DrumSection> sections;
    std::vector<daw::DrumPattern> added;   // library patterns not yet on the track
    int selected = -1;
    bool updating = false;
    double suggestedBpm = 0.0;

    juce::Label sectionsTitle, libraryTitle, autoTitle, nameLabel, fromLabel, barsLabel, grooveLabel, fillLabel, everyLabel,
        styleLabel, kindLabel, autoStyleLabel, autoBarsLabel, autoResult, info;
    std::unique_ptr<SectionList> sectionModel;
    std::unique_ptr<LibraryList> libraryModel;
    juce::ListBox sectionBox, libraryBox;
    juce::TextButton addButton, duplicateButton, removeButton, asGroove, asFill, autoButton, writeButton, closeButton;
    juce::TextEditor nameEditor;
    juce::Slider fromSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 barsSlider { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 autoBars { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::ComboBox grooveBox, fillBox, everyBox, styleBox, kindBox, autoStyle;
    juce::ToggleButton crashToggle, tempoToggle, followChords;
    juce::Random random;
};
} // namespace nam::ui
