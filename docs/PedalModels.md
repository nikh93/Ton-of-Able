# Gilmour original A1 rig collection

Downloaded 2026-09-15 for the user's personal installation. **All three original models are retained**, with original filenames and unchanged bytes. Selected from the uploader's Gilmour-themed pack; not listening-tested or asserted to reproduce a particular recording.

## Provenance and license

Original upload: [GILMOUR TONE, tone 5933](https://www.tone3000.com/tones/gilmour-tone-5933), by [pinkflor](https://www.tone3000.com/pinkflor). This source applies to every file below. Although the page labels the whole pack Pedal Capture, its three entries explicitly include a HIWATT DR103 amp alongside two pedals. Do not classify the Hiwatt as a pedal based on that pack-level tag.

Normal browser flow: A1 Legacy → Download all A1 → Just download tone. No account creation, payment, or access bypass. Original downloaded archive copied intact to:

`D:\Amp Modeler\downloads\pedal-rigs\GILMOUR TONE - A1 original.zip`

Archive: 890910 bytes; SHA-256 `87CAEF705FA90CE27DF53C3CEF50A0E5D8F2556CD17E0432C6E5FD45BB7A93AB`.

The browser's initial copy remains at `C:\Users\nikh\Downloads\GILMOUR TONE.zip`.

**T3K license**, displayed on the original page: downloading and software use are permitted, as is publishing resulting audio; uploading, republishing, or distributing the model files requires the author's permission. Local personal bundling is the scope here, not public installer redistribution. [Source sharing policy](https://www.tone3000.com/guides/tone-sharing-guidelines#tone-sharing-policy).

## Exact files and rig mapping

| Original capture | Role | Exact installed path |
| --- | --- | --- |
| TUBE DRIVER BUTLER BOOST | Pedal: Butler Tube Driver boost | `D:\Amp Modeler\resources\pedal-models\TUBE DRIVER BUTLER BOOST.nam` |
| big muff | Pedal: Big Muff; variant unspecified | `D:\Amp Modeler\resources\pedal-models\big muff.nam` |
| HIWATT DR103 IMG 1228 500EP | Amp component, retained from the same original pack | `D:\Amp Modeler\resources\rig-models\HIWATT DR103 IMG 1228 500EP.nam` |

For the main agent's rig selector, group these three resources under **GILMOUR TONE — pinkflor — original A1 collection**. This is a collection of separate captures, **not one combined pedal+amp NAM or a verified full-rig capture**.

Suggested audition mappings (application choices, not uploader-specified presets): Tube Driver Boost → original Hiwatt, or Big Muff → original Hiwatt. Keep both pedals associated with the collection. The source does not specify stacking order, so do not label a particular two-pedal chain as the original captured rig. No combined NAM was synthesized.

Neither pedal replaces an amp/cab stage. The Hiwatt filename identifies an amp, but the source supplies no cabinet/microphone description and the file has no capture-type metadata. Its cabinet inclusion is therefore **unverified**: expose the IR choice and audition with a suitable external cabinet IR rather than asserting that a cabinet is embedded. No IR was included in the archive. A selector calling this a complete rig must make the unresolved cabinet stage clear.

Physical knob positions, reamp chain and calibration levels are not provided. `500EP` is part of the original Hiwatt filename, not independently verified training metadata. Do not equate the numeric `gain` metadata below with amp/pedal knob settings.

## Validation and exact embedded metadata

All three parse as real NAM JSON, version **0.5.1**, architecture **WaveNet**, with **13802 finite numeric weights** each. Conventional A1 Standard topology: mono input/output, two layer arrays with 16/8 channels, kernel size 3, Tanh, ungated, dilations 1/2/4/8/16/32/64/128/256/512 per array, head scale 0.02, no separate head. No A2/slimmable container.

**Sample rate is absent from all three originals.** 48 kHz is an audition target, not certified source metadata. Do not silently label these as verified 48 kHz captures or rewrite the files. Runtime/audio validation remains with the main agent; no builds or listening tests were run here.

Complete `metadata` values (no other embedded metadata fields):

| File | Date fields (timezone unspecified) | loudness | gain |
| --- | --- | ---: | ---: |
| TUBE DRIVER BUTLER BOOST.nam | year=2023, month=4, day=25, hour=10, minute=11, second=51 | -20.222736358642578 | 0.3967987037983002 |
| big muff.nam | year=2023, month=4, day=23, hour=14, minute=42, second=41 | -11.195289611816406 | 0.4459207387755415 |
| HIWATT DR103 IMG 1228 500EP.nam | absent | -6.018765568733215 | absent |

No embedded ESR, training-check result, modeled-by, physical settings, input/output dBu calibration, or cabinet identity is present. Author and gear attribution come from the upload page and original filenames.

## Integrity and duplicate check

| File | Bytes | File SHA-256 |
| --- | ---: | --- |
| TUBE DRIVER BUTLER BOOST.nam | 297008 | `AAC7EB5B82A0AB31CE493CA67FB45DC6CF222CE792803796C122EA3D95D4D520` |
| big muff.nam | 297122 | `5769AC778EE327F8C699D74A1A2DFDE54E33B02AA470CD0BDD3CE3FE8E36D717` |
| HIWATT DR103 IMG 1228 500EP.nam | 296388 | `73A9ACFCB968E65AF8E92F7EDB848D5EE5BBF1E0CCB591E6327B77ABB12901BA` |

Weight-only SHA-256 (parsed weights serialized sequentially as IEEE-754 float64, little-endian, with no prefix) confirms distinct weight arrays, not just distinct filenames/metadata:

- Tube Driver: `293030DBB9FFE695ED5BD71A8DDCC964AC3810D954C14B33991080221B170696`
- Big Muff: `46264ACCC857A3B9835B8036F1FFE1C02C06239B369AFA63056418BA7364E1EB`
- Hiwatt: `6685A492A73E2C4F2221C56DCDD3E6253D8169B04DD2C2E4FFE21A3E82F61B5C`

## Additional sources researched, not installed

[Tube Driver Butler Boost & Drive 400EP, pinkflor](https://www.tone3000.com/tones/tube-driver-butler-boost-drive-400ep-6311) lists clean boost and drive captures; no second Tube Driver archive was downloaded, and no potential duplicate boost was added. [RR Bighorn Fuzz, RobyRocks](https://www.tone3000.com/tones/rr-bighorn-fuzz-46432) describes free Wren & Cuff Caprid / Ram's Head-style pedal snapshots. Neither source is part of this delivered three-model set.

No source, CMake, discovery, selector, or test files were edited for this download task. The main agent owns application integration and personal-install copying.
