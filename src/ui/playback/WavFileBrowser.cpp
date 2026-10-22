#include "WavFileBrowser.h"

#include "../components/AmberTheme.h"

#include <algorithm>

namespace nam::ui
{
WavFileBrowser::WavFileBrowser()
{
    list.setRowHeight(30);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(list);
}

void WavFileBrowser::setDirectory(const juce::File& directory)
{
    currentDirectory = directory;
    refresh();
}

void WavFileBrowser::refresh()
{
    wavFiles.clear();
    if (currentDirectory.isDirectory())
        currentDirectory.findChildFiles(wavFiles, juce::File::findFiles, false, "*.wav");
    std::sort(wavFiles.begin(), wavFiles.end(), [](const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareNatural(b.getFileName()) < 0;
    });
    list.updateContent();
    repaint();
}

int WavFileBrowser::getNumRows() { return wavFiles.size(); }

void WavFileBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (!juce::isPositiveAndBelow(row, wavFiles.size())) return;
    auto box = juce::Rectangle<float>(0, 1, float(width), float(height - 2)).reduced(2, 0);
    if (selected)
    {
        g.setGradientFill(juce::ColourGradient(AmberTheme::amber().withAlpha(.48f), 0, box.getY(),
            juce::Colour(0xff845126).withAlpha(.45f), 0, box.getBottom(), false));
        g.fillRoundedRectangle(box, 6);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(.045f));
        g.fillRoundedRectangle(box, 6);
    }
    g.setColour(selected ? AmberTheme::amberBright() : AmberTheme::text());
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(wavFiles.getReference(row).getFileNameWithoutExtension(), box.reduced(10, 0), juce::Justification::centredLeft, true);
}

void WavFileBrowser::selectedRowsChanged(int row)
{
    if (juce::isPositiveAndBelow(row, wavFiles.size()) && onFileSelected)
        onFileSelected(wavFiles.getReference(row));
}

void WavFileBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow(row, wavFiles.size()) && onFileSelected)
        onFileSelected(wavFiles.getReference(row));
}

void WavFileBrowser::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff79908b).withAlpha(.26f), 0, 0,
        juce::Colour(0xff111d1a).withAlpha(.62f), 0, r.getBottom(), false));
    g.fillRoundedRectangle(r, 8);
    g.setColour(juce::Colours::white.withAlpha(.13f));
    g.drawRoundedRectangle(r.reduced(.5f), 8, 1);
    if (wavFiles.isEmpty())
    {
        g.setColour(AmberTheme::subduedText());
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText(currentDirectory.isDirectory() ? "Keine WAV-Dateien in diesem Ordner" : "WAV-Ordner auswählen", getLocalBounds().reduced(12), juce::Justification::centred, 2);
    }
}

void WavFileBrowser::resized() { list.setBounds(getLocalBounds().reduced(5)); }
}
