# UI components

The classes in this folder are small, reusable JUCE widgets. They do not depend on
`MainComponent`, the audio engine, or the CMake target.

## `AmberTheme`

`AmberTheme` provides the warm amber palette and `applyTo(juce::LookAndFeel_V4&)`
for configuring a local or application-wide JUCE look-and-feel.

## `RotaryParameterControl`

`RotaryParameterControl` combines a title, rotary slider and value readout. Set its
range with `setRange`, optionally supply `setValueFormatter`, and connect
`onValueChange` to a UI-to-parameter handoff. `slider()` is available when a JUCE
attachment is preferable.

## `AudioMeter`

`AudioMeter` accepts a pointer to an audio-thread-owned `std::atomic<float>` peak
value. The audio callback only writes that atomic; the component polls it at 30 Hz
on JUCE's message thread and performs drawing there. Peak values are expected in
the linear `[0, 1]` range.
