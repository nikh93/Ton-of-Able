#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
class WavFileBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    WavFileBrowser();

    void setDirectory(const juce::File& directory);
    [[nodiscard]] const juce::File& directory() const noexcept { return currentDirectory; }
    void refresh();
    std::function<void(const juce::File&)> onFileSelected;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    juce::ListBox list { "WAV files", this };
    juce::File currentDirectory;
    juce::Array<juce::File> wavFiles;
};
}
