# NAM Test Program Tests

`TestMain.cpp` and `NamTestProgramTests.cpp` form a deterministic C++20 JUCE
unit-test executable. They deliberately do not alter the application target or
its CMake configuration.

To integrate them, add a separate JUCE console-app target in the consumer's
CMake configuration (or a local, uncommitted test overlay) and include both
test source files plus these application implementation files:

```
src/library/LibraryManager.cpp
src/presets/PresetManager.cpp
src/playback/FilePlayer.cpp
src/diagnostics/PerformanceMonitor.cpp
```

Give that target `src` as an include directory, define `JUCE_UNIT_TESTS=1`,
and link `juce::juce_audio_formats`, `juce::juce_dsp`,
`juce::juce_data_structures`, and their normal JUCE dependencies. The effect
tests are header-only and require no effect implementation source.

The suite creates its WAV and library-link fixtures under the operating
system's temporary directory, removes them automatically, uses no real audio
devices, and makes no timing-duration assertions. This keeps tests independent
of the checked-out workspace and stable under CI load.
