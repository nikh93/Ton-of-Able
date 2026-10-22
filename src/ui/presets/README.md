# Preset-management UI

`PresetManagementComponent` is a reusable JUCE message-thread component over a
non-owning `PresetManager`. Call `setPresetManager()` and `refresh()` after the
application changes the library. Its callbacks deliberately delegate state
mutation and DSP publication to the owning controller:

- `onSave(id)`, `onSaveAs(name)`, `onLoad(id)`, `onRename(id, name)`, `onDelete(id)`
- `onABSelectionChanged(useA)`

The selector uses `Preset::id`, not list positions. The component applies the
shared `AmberTheme` palette and does no file I/O or audio-thread work.
