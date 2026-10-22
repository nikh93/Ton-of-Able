# Diagnostics panel

`DiagnosticsPanel` is a compact JUCE component for displaying the copied
`PerformanceMonitor::Snapshot` and recent `Logger::Entry` values. Construct it
with providers such as:

```cpp
nam::ui::DiagnosticsPanel panel(
    [&monitor] { return monitor.snapshot(); },
    [&logger]  { return logger.recentEntries(); });
```

The providers are invoked only by the component's `juce::Timer` on the message
thread (default: 500 ms). The component never holds or dereferences an audio
engine, callback buffer, or mutable audio-thread object. `PerformanceMonitor`
returns a lock-free value snapshot; `Logger` returns its synchronised value
copy. No timer callback is executed on the audio thread.

Styling uses the shared `AmberTheme` palette. The panel shows realtime load,
average/peak callback time, processed blocks/XRuns, and the six newest log
entries.
