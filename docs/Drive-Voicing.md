# Drive voicing revision

## Controls and gain staging

Input sets the guitar level entering the pedal. Drive/Gain sets pedal saturation.
Level is a linear stored gain, now displayed in dB (1.0 = 0 dB). AMP PUSH is a
separate -18 to +18 dB trim immediately before NAM; it also works with the pedal
bypassed. It does not affect the NAM bypass signal. OUTPUT sets listening volume.
AMP PUSH is stored in presets/A-B; old presets default to 0 dB.

The new pedal uses 0.20 peak (-14 dBFS) as an internal digital reference.
This is deliberately not described as analog calibration: the bundled captures
do not contain input/output calibration metadata, and the interface reference
level is unknown. AMP PUSH defaults to 0 dB, including the factory drive presets.
Use it to set the amp's breakup independently of pedal distortion.

## Voices

- Warm Blues: bass shelving before saturation, broad mid emphasis, continuous
  asymmetric soft saturation, a small clean component, two-pole treble filtering.
  Gain spans 3–30 dB internally, so quiet DI signals can still overdrive it.
- Singing Lead: filtered first stage, DC coupling filter and treble reduction
  between stages, gentler second stage with opposite asymmetry. Sustain uses an
  8 ms attack and 180–320 ms release to lift fading notes by at most 9.5 dB.
  It cannot create an infinite note and has no hard noise gate.
- Crunch's zero-crossing discontinuity is removed; Fuzz's idle offset is removed.
- A 12 Hz output DC blocker prevents bias from being passed into NAM.
- 4x oversampling retained. Dry/wet mixing now shares the oversampling path and
  its latency, including in bypass. These are voiced effects, not exact circuit
  simulations or newly trained NAM captures.

All processing storage is prepared outside the callback. The engine uses a
non-owning view of the actual callback length, fixing stale extra samples when a
callback is shorter than the prepared maximum.

## Verification

DriveVoicingTests checks quiet-signal distortion, picking dynamics, late/early
note energy, silence and DC, mix alignment, callback-size invariance and trim
persistence. Existing tests cover four sample rates and several block sizes.
These checks do not establish subjective sound quality.

At 48 kHz, the current sine test measures harmonic/fundamental ratios of 0.0171
(low drive), 0.0668 (quiet picking at the Warm Blues setting), and 0.2672 (strong
picking). The Lead late/early RMS ratio increases from 0.1222 to 0.3300 with
Sustain. These are behavioral checks, not perceptual ratings. All 28 test
sections passed in the release build.

`DriveAudition` is an optional build target that renders an input WAV through
Drive, an actual NAM model, and a cabinet IR. It writes bypassed, Warm Overdrive,
and Singing Lead examples as 48 kHz/24-bit mono WAVs, using one fixed input
reference and equal output RMS. This matching happens only in the offline
comparison; the live application does not auto-normalize playing dynamics.

The generated `docs/drive-audition-twin/` examples use `TestAudio/audio.wav`,
Fender TwinVerb Norm Bright and Jester Emerald / Nacho Guacamole. Each file is
9.294 seconds including its tail, matched to -22.155 dBFS RMS. WAV peak levels
are 0.850 (bypass), 0.553 (Warm), and 0.575 (Lead). All files passed readback and
finite-sample verification. The input recording's provenance is not known;
the tool assumes the metadata-free NAM capture runs at 48 kHz. No listening
assessment is claimed.

For listening comparisons, use the same DI, NAM, and cabinet with matched output
RMS. Start with Warm Blues or Singing Lead, AMP PUSH at 0 dB, and a clean/edge
capture; compare with drive bypass at matched listening level. Turn AMP PUSH up
only if more amp saturation is wanted. Previously saved custom drive settings
remain stored but sound different with the revised DSP.

## Design references

- https://www.electrosmash.com/tube-screamer-analysis
- https://www.wamplerpedals.com/blog/music/2020/05/gain-stacking-101/
- https://www.neuralampmodeler.com/post/neuralampmodelerplugin-v0-7-12-is-released
- https://www.valvewizard.co.uk/Common_Gain_Stage.pdf

The references inform the topology and gain contract; they do not validate a
claim that these DSP voices reproduce the named hardware exactly.
