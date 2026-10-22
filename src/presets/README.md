# Preset persistence and A/B state

`SoundState` is the versioned, complete sound schema: linked NAM/IR identifiers, gains, EQ, every effect parameter, and every bypass. `PresetManager` stores named, timestamped copies of that state. `ABState` holds two complete independent `SoundState` values plus the selected side.

All objects round-trip through `juce::ValueTree`. `PresetManager::valueTreeToJson` and `valueTreeFromJson` provide a JSON-compatible representation for tests or alternate settings backends. Applying a decoded state to DSP is intentionally outside this module: the controller must prepare model/IR changes off the audio thread before publishing a state.
