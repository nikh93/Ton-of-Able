# Bundled default NAM models

These four model files come from the public `pelennor2170/NAM_models`
collection and are distributed under GPL-3.0 as documented by that project.
The complete licence text is included as `GPL-3.0.txt`.

- `Phillipe P Bug333-Clean-ESR0,002.nam` – clean
- `Phillipe P Bug333-Crunch-DT-ESR0,0133.nam` – crunch
- `Helga B JSX-Crunch-NoBoost-0,004.nam` – classic-rock crunch
- `Helga B 6505+ Red Ch - NoBoost rock.nam` – high gain

Source collection: https://github.com/pelennor2170/NAM_models

The application links these files into the user's NAM library on first start;
the files are never copied or modified after installation.

## Bundled cabinet IRs

The WAV files below are from Jester Dyne Productions' free packs. The Brutal
Pack is stated as CC0/public domain; the publisher states that both packs are
free, including for commercial use. The source pages and pack descriptions are
kept in `resources/DEFAULT_ASSETS.md`.

- `Jester Brutal - Cookie Monster 48k.wav`
- `Jester Brutal - Vertical Lip Stabber 48k.wav`
- `Jester Emerald - Nacho Guacamole 48k.wav`

Source pages:

- https://www.jester-dyne-productions.com/brutal-ir-pack/
- https://www.jester-dyne-productions.com/emerald-ir-pack/

## TONE3000 models and IRs (`T3K …`)

Downloaded from TONE3000 by the project owner and bundled for personal use.
All twelve models are full-rig captures (amp and cabinet), so the app switches
the IR stage off when one is selected. Authors are credited in the file names
and in `curated_catalog.json` (`source`, `sourceUrl`). **TONE3000 licences
differ per tone; the standard T3K licence does not allow redistribution
without the author's permission. Check each page before publishing builds or
the repository.**

| Files | Author | Source |
| --- | --- | --- |
| `T3K Mad Hatter - Mesa 90s Dual Rec Red Modern …` (2) | Mad Hatter Amp Lab | https://www.tone3000.com/tones/mesa-90s-dual-rectifier-ri-red-modern-full-rig-87866 |
| `T3K jpisoutoftune - 5150 Stealth … Mesa OS` (Blue, Red, Green SD1) | jpisoutoftune | https://www.tone3000.com/tones/5150-stealth-100w-mesa-os-full-rig-blue-red-and-green-84864 |
| `T3K amalgamaudio - Vox AC30 Top Boost 1964 Edge of Breakup A2` | amalgamaudio | https://www.tone3000.com/tones/1964-vox-ac30-top-boost-super-twin-edge-of-breakup-a2-82521 |
| `T3K ronkiballe - Twin Reverb 67 …` (6) | ronkiballe | https://www.tone3000.com/tones/fender-twin-reverb-67-srv-94685 |
| `T3K Marshall JCM900 1960 4x12 - …` (SM57, Sennheiser e609, Blended; cabinet IRs) | see source page | https://www.tone3000.com/tones/marshall-jcm900-lead-1960-cab-90295 |
| `T3K Sunset Sound Studio 3 - Live Room` (room IR, mono) | see source page | https://www.tone3000.com/tones/studio-3-live-room-99227 |

Not bundled: `S.R.V_get out of jail free card_X.nam` from the Twin pack is
byte-identical to `BF_TWIN_67_SRV_Lenny_Duellist_Glass_X.nam`, and the stereo
version of the Live Room IR is omitted because the rig is mono.
