# Recommended default NAM and cabinet assets

Research date: 2026-09-15. This is a curation list, not a vendored asset
manifest. A `.nam` capture has its own licence; the licence of
NeuralAmpModelerCore does **not** grant permission to distribute a capture.

## Recommended NAM models

All four recommendations below are from the public
[`pelennor2170/NAM_models`](https://github.com/pelennor2170/NAM_models)
collection. The collection's README explicitly licenses its model files under
GPL-3.0. They are therefore redistributable, provided that the distributed
model files retain the GPL-3.0 notice/text and an accessible copy/source URL.
The creator attribution is encoded in the upstream filenames.

| Default role | Model / creator | Source | Licence / redistribution constraint | Sound category | Why this is the default |
| --- | --- | --- | --- | --- | --- |
| Clean | `Phillipe P Bug333-Clean-ESR0,002.nam` — Phillipe P | [upstream file](https://github.com/pelennor2170/NAM_models/blob/main/Phillipe%20P%20Bug333-Clean-ESR0%2C002.nam) | GPL-3.0; **may bundle** with `COPYING`/GPL-3.0 text and the upstream source link. | Clean amp head capture, no cabinet baked in. | A clearly labelled clean, cab-free starting point; it makes the program's separate IR stage immediately understandable. |
| Crunch | `Phillipe P Bug333-Crunch-DT-ESR0,0133.nam` — Phillipe P | [upstream file](https://github.com/pelennor2170/NAM_models/blob/main/Phillipe%20P%20Bug333-Crunch-DT-ESR0%2C0133.nam) | GPL-3.0; **may bundle** with `COPYING`/GPL-3.0 text and the upstream source link. | Crunch / edge-of-breakup amp head capture, no cabinet baked in. | Same capture family as the clean choice but a distinct gain setting: useful for validating level, model switching and the gain-range progression without changing unrelated variables. |
| Classic rock | `Helga B JSX-Crunch-NoBoost-0,004.nam` — Helga B | [upstream file](https://github.com/pelennor2170/NAM_models/blob/main/Helga%20B%20JSX-Crunch-NoBoost-0%2C004.nam) | GPL-3.0; **bundled** under `resources/defaults` with the GPL-3.0 text and source URL. | British-style classic-rock crunch, no boost. | A distinct mid-forward crunch reference between the Bug333 pair and the high-gain 6505+ model. |
| High gain | `Helga B 6505+ Red Ch - NoBoost rock.nam` — Helga B | [upstream file](https://github.com/pelennor2170/NAM_models/blob/main/Helga%20B%206505%2B%20Red%20Ch%20-%20NoBoost%20rock.nam) | GPL-3.0; **may bundle** with `COPYING`/GPL-3.0 text and the upstream source link. | High-gain 6505+ red-channel amp head capture; no external boost. | A named high-gain reference with the boost deliberately absent, so users can hear and test the program's own front-end/effects choices. |

### NAM bundling note

Bundling these four files is legally practical, but only if the installer or
asset directory also includes the GPL-3.0 text, preserves the filenames and
attribution, and points to the source collection. Do not infer that a model
from another NAM host is redistributable merely because it is free to
download. For example, TONE3000's standard T3K licence permits use and output
publication but prohibits uploading, republishing, or distributing the data
file without the author’s permission.

### TONE3000 bundle (personal use)

Twelve TONE3000 full-rig captures and four IRs (`T3K …` in
`resources/defaults`) were supplied by the project owner and are bundled
while the repository is private. Their redistribution terms were **not**
verified; see `resources/defaults/README.md` for authors and source pages,
and check each tone's licence before any public release.

## Bundled cabinet IRs

The initial install includes three 48 kHz WAV IRs from Jester Dyne
Productions' free packs: `Jester Brutal - Cookie Monster`, `Jester Brutal -
Vertical Lip Stabber`, and `Jester Emerald - Nacho Guacamole`. The publisher
states that the packs are free, including for commercial use; the Brutal Pack
is additionally described as CC0/public domain. They are automatically linked
into the IR library on first start.

## Additional recommended cabinet IRs

The current free [Redwirez Marshall 1960 IR Pack](https://redwirez.com/pages/the-marshall-1960a-ir-pack)
is the most suitable common source for the initial three IR choices: it is a
Marshall 1960A loaded with Celestion G12M25 Greenbacks and provides SM57,
Royer R121, Sennheiser MD421 and Neumann KM84 microphones. Its publisher is
explicit: the pack may **not** be redistributed in whole or part; people must
be linked to the product page. Consequently **none of these IRs may be
bundled** with the program. Present them as optional user-download links only.

| Default role | IR / creator | Source | Licence / redistribution constraint | Sound category | Why this is the default |
| --- | --- | --- | --- | --- | --- |
| Bright cut | Marshall 1960A G12M25 — SM57, close/cap position — Redwirez | [free pack](https://redwirez.com/pages/the-marshall-1960a-ir-pack) | Free newsletter-download; **user-download link only, no redistribution**. | Tight, forward close-mic Greenback 4x12. | The practical bright reference for crunchy riffs and high-gain attack; useful for exposing pick definition and fizz-control behaviour. |
| Balanced body | Marshall 1960A G12M25 — Royer R121, close/cap position — Redwirez | [free pack](https://redwirez.com/pages/the-marshall-1960a-ir-pack) | Free newsletter-download; **user-download link only, no redistribution**. | Fuller, smoother ribbon-mic Greenback 4x12. | A deliberately contrasting ribbon option that tames the top end of the high-gain model and provides a strong rock lead baseline. |
| Open/clean detail | Marshall 1960A G12M25 — Neumann KM84, edge or mid-distance position — Redwirez | [free pack](https://redwirez.com/pages/the-marshall-1960a-ir-pack) | Free newsletter-download; **user-download link only, no redistribution**. | Detailed condenser-mic Greenback 4x12. | Gives clean and lower-gain models more air and articulation than the two close-mic defaults, making IR changes audibly educational. |

## Source and compliance checks

- [NAM_models README and GPL-3.0 declaration](https://github.com/pelennor2170/NAM_models#readme)
- [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) is MIT-licensed software, but its licence applies to the DSP library rather than third-party `.nam` captures.
- [Redwirez free-pack terms](https://redwirez.com/pages/the-marshall-1960a-ir-pack) state both the free-download mechanism and the no-redistribution requirement.
- [TONE3000 T3K licence example](https://www.tone3000.com/tones/aa-6-snapshots-pack-6491) is included as a counterexample: free use does not equal redistribution permission.

Before release, verify each selected upstream file still exists and preserve a
copy of the exact licence text alongside every bundled NAM asset. Keep all
Redwirez assets out of the repository, installers and binary resources.
