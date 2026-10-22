# AppSettings

`AppSettings` persists non-realtime application state through JUCE's
`PropertiesFile` (XML storage). It owns no audio-thread state and must only be
used from the message thread or a dedicated persistence worker.

Persisted values are:

- the `AudioDeviceManager::createStateXml()` result;
- selected device names, type, sample rate and buffer size;
- NAM and IR library `ValueTree` roots;
- the preset-library `ValueTree` root and last selected preset UUID;
- window bounds; and
- a schema-free UI `ValueTree` for view-specific preferences.

The audio device XML should be loaded before calling
`AudioDeviceManager::initialise`. Save it only after device configuration has
settled. Call `flush()` during graceful shutdown if immediate persistence is
required; normal writes are batched by `PropertiesFile`.

The class deliberately does no locking: sharing an instance between UI and
worker threads requires external serialization. Never access it from the audio
callback, since XML serialization and `PropertiesFile` writes can allocate and
perform disk I/O.
