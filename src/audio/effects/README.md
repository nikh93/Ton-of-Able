# Effects DSP

Header-only C++20 effects for the NAM audio chain. They are intended for one or two channel blocks; prepare them once from `audioDeviceAboutToStart`, then call `process()` only from the audio callback. `prepare()` is the only operation which allocates (the chorus and delay lines).

Suggested signal order is: **input → AutoWah → EQ → NAM model → Chorus → Delay → Reverb → output**. This keeps the amp model in the familiar pedal-before-amp position and places time effects after it.

## API

Every processor has `prepare(const juce::dsp::ProcessSpec&)`, `reset()`, `setBypassed(bool)`, and `process(juce::dsp::AudioBlock<float>)` / `process(juce::AudioBuffer<float>&)`. Setters are safe to call from the message thread: they only write atomics. The audio callback consumes those values and performs the smoothing ramps itself, so it takes no locks and performs no allocation.

| Processor | Parameters |
| --- | --- |
| `EQProcessor` | `setLowGainDb`, `setMidGainDb`, `setHighGainDb` |
| `AutoWahProcessor` | `setSensitivity`, `setResonance` |
| `ChorusProcessor` | `setRateHz`, `setDepth`, `setMix` |
| `DelayProcessor` | `setTimeSeconds`, `setFeedback`, `setMix` |
| `ReverbProcessor` | `setRoomSize`, `setDamping`, `setMix` |

All bypasses crossfade over 10 ms. Parameter setters clamp inputs to musical, stable ranges.
