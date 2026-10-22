# Ton of Able

Standalone Windows application for testing Neural Amp Modeler (`.nam`) models in real time.

## Current status

Personal extensions include a two-slot VST3 host and a separate asynchronous
NAM pedal stage before the NAM amp. See [NAM pedal usage](docs/NAM-Pedals.md)
and [VST3 rack usage](docs/VST3-Rack.md). Pedal input/output trims, bypass and
model path are persisted independently in presets and A/B.

Ton of Able also contains an Ableton-style **multitrack arrangement**: real
multitrack recording from the NAM rig or dry interface inputs (count-in,
metronome, latency compensation), clip editing (move, copy, trim, split, fades,
undo/redo, snap, Ctrl+mouse-wheel zoom), time-stretching and pitch-shifting with
the [Rubber Band Library](https://breakfastquay.com/rubberband/), per-track
effect chains (NAM, IR, EQ, graphic EQ, effects, VST3) as insert or printed
while recording, a 10-band graphic EQ, audio/video import and mix/stem export
in common formats (FFmpeg for AAC, Opus, video, ...), a step-sequencer drum
machine (built-in kit or any General MIDI VST3 drum instrument such as
[JDrummer](https://github.com/jmantra/jdrummer)) and live pitch shifting with
Rubber Band (e.g. an octave down) in front of the rig, send effects on up to
four return tracks, track freezing (with "freeze and flatten"), a compressor
and a wah pedal (playable with the vertical mouse position) in every effect
chain, and freely assignable keyboard shortcuts (F1). See
[Arrangement](docs/Arrangement.md).

The complete realtime core and the primary user workflow are implemented:
ASIO-first/WASAPI audio I/O, mono DSP routing, asynchronous NAM and IR loading,
EQ, Auto-Wah, Chorus, Delay, Reverb, WAV playback, threaded recording,
persistent NAM/IR libraries with multi-file/folder import into a model folder
(imported models are copied to `Documents/Ton of Able/Modelle`, see below), preset Save/Save As/load/rename/delete, A/B state
switching, effect parameter panels, clipping meters, diagnostics and persistent
settings. Thirty-one GPL-3.0 curated NAM configurations are bundled under
[resources/defaults](resources/defaults) and registered in the library on first
start. The full curated catalog is defined in
[resources/defaults/curated_catalog.json](resources/defaults/curated_catalog.json)
and exposes Amp/Configuration selection while retaining technical filenames
only in the library view. The curated IR list is documented in
[resources/DEFAULT_ASSETS.md](resources/DEFAULT_ASSETS.md); third-party IRs are
not redistributed where their licence forbids it.
Twelve TONE3000 full-rig captures (Mesa Dual Rectifier, EVH 5150 Stealth,
Vox AC30 1964, Fender Twin Reverb '67 SRV) and four TONE3000 IRs (Marshall
JCM900 1960 4x12, Sunset Sound Studio 3 live room) are bundled for personal
use; see [resources/defaults/README.md](resources/defaults/README.md) before
publishing.

**Model folder.** *NAM importieren* / *IR importieren* accept several files at
once or whole folders (including subfolders); `.nam` files and folders can also
be dropped onto the rig window. Every imported file is copied into the model
folder (default `Documents/Ton of Able/Modelle/NAM` and `.../IR`), so library
entries keep working when the downloads are moved or deleted. Identical files
are not copied twice, a different file with the same name gets a numbered name,
and files put into the folder by hand are picked up at start-up (or with
*Modellordner neu einlesen*). The import menu can also open or change the
folder and copy previously linked external models into it.

## Build prerequisites

- Windows 10 or 11
- CMake 3.24 or newer
- Visual Studio 2022 Build Tools or Visual Studio 2022 with the **Desktop development with C++** workload and Windows SDK
- A Steinberg ASIO SDK-compatible installation/licensing arrangement when shipping ASIO support

## Configure and build

Third-party code is pulled in as git submodules (JUCE, NeuralAmpModelerCore,
Rubber Band):

```powershell
git submodule update --init --recursive
```

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
