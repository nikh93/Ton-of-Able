#include "DiagnosticsPanel.h"

#include "../components/AmberTheme.h"

namespace nam::ui
{
namespace
{
constexpr int maximumVisibleEntries = 6;

juce::String formatMilliseconds(double value)
{
    return juce::String(value, value < 10.0 ? 2 : 1) + " ms";
}
}

DiagnosticsPanel::DiagnosticsPanel(PerformanceProvider newPerformanceProvider,
                                   LogProvider newLogProvider,
                                   int refreshIntervalMs)
    : performanceProvider(std::move(newPerformanceProvider)),
      logProvider(std::move(newLogProvider))
{
    jassert(performanceProvider != nullptr);
    jassert(logProvider != nullptr);

    for (auto* label : { &loadLabel, &timingLabel, &blockLabel })
    {
        label->setColour(juce::Label::textColourId, AmberTheme::text());
        label->setJustificationType(juce::Justification::centredLeft);
        label->setFont(juce::FontOptions { 13.0f });
        addAndMakeVisible(*label);
    }

    logView.setMultiLine(true);
    logView.setReadOnly(true);
    logView.setScrollbarsShown(true);
    logView.setCaretVisible(false);
    logView.setColour(juce::TextEditor::backgroundColourId, AmberTheme::surface());
    logView.setColour(juce::TextEditor::textColourId, AmberTheme::subduedText());
    logView.setColour(juce::TextEditor::outlineColourId, AmberTheme::surfaceRaised());
    logView.setFont(juce::FontOptions { 12.0f });
    addAndMakeVisible(logView);

    refresh();
    startTimer(juce::jmax(100, refreshIntervalMs));
}

DiagnosticsPanel::~DiagnosticsPanel()
{
    stopTimer();
}

void DiagnosticsPanel::paint(juce::Graphics& graphics)
{
    graphics.setColour(AmberTheme::surfaceRaised());
    graphics.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
    graphics.setColour(AmberTheme::amber().withAlpha(0.7f));
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);

    graphics.setColour(AmberTheme::amberBright());
    graphics.setFont(juce::FontOptions { 13.0f, juce::Font::bold });
    graphics.drawText("DIAGNOSE", 10, 7, getWidth() - 20, 18, juce::Justification::centredLeft);
}

void DiagnosticsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    bounds.removeFromTop(22);
    loadLabel.setBounds(bounds.removeFromTop(20));
    timingLabel.setBounds(bounds.removeFromTop(20));
    blockLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(4);
    logView.setBounds(bounds);
}

void DiagnosticsPanel::refresh()
{
    if (performanceProvider != nullptr)
    {
        const auto snapshot = performanceProvider();
        loadLabel.setText("Echtzeit-Auslastung: " + juce::String(snapshot.realtimeLoad * 100.0, 1) + " %", juce::dontSendNotification);
        timingLabel.setText("Block: " + formatMilliseconds(snapshot.averageMilliseconds)
                                + " Mittel / " + formatMilliseconds(snapshot.peakMilliseconds) + " Spitze",
                            juce::dontSendNotification);
        blockLabel.setText(juce::String(juce::CharPointer_UTF8("Bl\xc3\xb6" "cke: ")) + juce::String(snapshot.processedBlocks)
                               + juce::String(juce::CharPointer_UTF8("   Aussetzer: ")) + juce::String(snapshot.xruns),
                           juce::dontSendNotification);
    }

    if (logProvider != nullptr)
    {
        const auto entries = logProvider();
        juce::StringArray lines;
        const auto first = juce::jmax(0, static_cast<int>(entries.size()) - maximumVisibleEntries);
        for (int index = first; index < static_cast<int>(entries.size()); ++index)
            lines.add(formatEntry(entries[static_cast<size_t>(index)]));
        logView.setText(lines.joinIntoString("\n"), false);
    }
}

void DiagnosticsPanel::timerCallback()
{
    refresh();
}

juce::String DiagnosticsPanel::formatEntry(const Logger::Entry& entry)
{
    juce::String level;
    switch (entry.level)
    {
        case Logger::Level::debug:   level = "DBG"; break;
        case Logger::Level::info:    level = "INF"; break;
        case Logger::Level::warning: level = "WRN"; break;
        case Logger::Level::error:   level = "ERR"; break;
    }
    return entry.timestamp.formatted("%H:%M:%S") + " [" + level + "] " + entry.text;
}

juce::Colour DiagnosticsPanel::colourForLevel(Logger::Level level) noexcept
{
    return level == Logger::Level::error || level == Logger::Level::warning
             ? AmberTheme::danger() : AmberTheme::subduedText();
}
} // namespace nam::ui
