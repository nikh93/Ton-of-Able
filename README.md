# Ton of Able

Standalone Windows application for testing Neural Amp Modeler (`.nam`) models in real time.

## Current status

Personal extensions include a two-slot VST3 host and a separate asynchronous
NAM pedal stage before the NAM amp. See [NAM pedal usage](docs/NAM-Pedals.md)
and [VST3 rack usage](docs/VST3-Rack.md). Pedal input/output trims, bypass and
model path are persisted independently in presets and A/B.

The complete realtime core and the primary user workflow are implemented:
ASIO-first/WASAPI audio I/O, mono DSP routing, asynchronous NAM and IR loading,
EQ, Auto-Wah, Chorus, Delay, Reverb, WAV playback, threaded recording,
persistent NAM/IR libraries, preset Save/Save As/load/rename/delete, A/B state
switching, effect parameter panels, clipping meters, diagnostics and persistent
settings. Thirty-one GPL-3.0 curated NAM configurations are bundled under
[resources/defaults](resources/defaults) and registered in the library on first
start. The full curated catalog is defined in
[resources/defaults/curated_catalog.json](resources/defaults/curated_catalog.json)
and exposes Amp/Configuration selection while retaining technical filenames
only in the library view. The curated IR list is documented in
[resources/DEFAULT_ASSETS.md](resources/DEFAULT_ASSETS.md); third-party IRs are
not redistributed where their licence forbids it.

## Build prerequisites

- Windows 10 or 11
- CMake 3.24 or newer
- Visual Studio 2022 Build Tools or Visual Studio 2022 with the **Desktop development with C++** workload and Windows SDK
- A Steinberg ASIO SDK-compatible installation/licensing arrangement when shipping ASIO support

## Configure and build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The application executable is written to `build/NamTestProgram_artefacts/<configuration>/Ton of Able.exe`.
The Release build uses the static MSVC runtime (`/MT`), so the application itself does not require the Visual C++ Redistributable. External VST3 plugins may still have their own dependencies.
Run the tests with:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```
