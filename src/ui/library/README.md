# Library browser

`nam::ui::LibraryBrowser` is a reusable JUCE component backed by a
`LibraryManager`, so it accepts both `NamLibrary` and `IRLibrary`. It presents
linked entries, tags and missing-file state in the application amber palette.

The browser never copies or deletes user audio/model files. Import, relink,
rename, tag and remove actions update the manager, then invoke their associated
`Callbacks` member. The host normally uses those callbacks to persist the
library and, for selection/relink, schedule any non-realtime model/IR loading.

Use `refresh()` after an external manager mutation, and `setSelectedItem()` to
restore a persisted selection. File chooser and dialog interaction always occurs
on the JUCE message thread; this component must not be touched by the audio
thread.
