#pragma once

#include "../../diagnostics/Logger.h"
#include "../../diagnostics/PerformanceMonitor.h"

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/**
    Compact, message-thread-only view of diagnostic snapshots.

    The panel deliberately receives pull functions instead of audio-engine
    objects. Its timer invokes them on JUCE's message thread; providers must
    return value snapshots and must not expose mutable audio-thread state.
*/
class DiagnosticsPanel final : public juce::Component,
                               private juce::Timer
{
public:
    using PerformanceProvider = std::function<PerformanceMonitor::Snapshot()>;
    using LogProvider = std::function<std::vector<Logger::Entry>()>;

    DiagnosticsPanel(PerformanceProvider performanceProvider,
                     LogProvider logProvider,
                     int refreshIntervalMs = 500);
    ~DiagnosticsPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Immediately refreshes the rendered values; must run on the message thread. */
    void refresh();

private:
    void timerCallback() override;
    static juce::String formatEntry(const Logger::Entry& entry);
    static juce::Colour colourForLevel(Logger::Level level) noexcept;

    PerformanceProvider performanceProvider;
    LogProvider logProvider;
    juce::Label loadLabel;
    juce::Label timingLabel;
    juce::Label blockLabel;
    juce::TextEditor logView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiagnosticsPanel)
};
} // namespace nam::ui
