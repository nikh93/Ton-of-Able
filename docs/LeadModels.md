# Lead NAM snapshots

Downloaded and structurally validated on 2026-09-15 for personal audition. Five distinct snapshots of one Dumble-style amp; these are candidates for warm singing/blues lead, not listening-tested recommendations.

## Original source and license

- Creator: [slamminmofo](https://www.tone3000.com/slamminmofo).
- Original upload for **all five files**: [Dumble ODS #102 Ford Hyper Accuracy+](https://www.tone3000.com/tones/dumble-ods-102-ford-hyper-accuracy-30435).
- Download method: source page → **A1 Legacy → Download all A1 → Just download tone**. The normal browser download worked; no account creation, payment, or access-restriction bypass was needed.
- Source license: **T3K**, shown on the upload page. It permits downloading, software use, and publication of resulting audio; distributing or republishing the model files requires the author's permission. These are personal-use assets, not cleared for inclusion in a distributed default catalog. See also [tone-sharing policy](https://www.tone3000.com/guides/tone-sharing-guidelines#tone-sharing-policy).
- Original A1 archive is also cached at `D:\Amp Modeler\downloads\Dumble-ODS-102-Ford-A1.zip` (browser original remains in Downloads). Only the five selected entries were extracted into the project, without altering their contents.
- The first generic Download Tone action supplied an A2 archive, retained at `C:\Users\nikh\Downloads\Dumble ODS #102 Ford Hyper Accuracy+.zip`; none of its models was installed in the project. A separate original Smooth A1 download remains in Downloads too.

## Capture and cabinet

The uploader identifies the amp as their **Dumble ODS #102 Robben Ford clone**, captured through the normal input and a Suhr Reactive Load. The chain includes a UA Apollo X8 and Lehle P-Split III. These are **AmpHead / amp-only**, not FullRig: enable a separate speaker cabinet IR for direct/headphone playback. No cabinet IR is included here. The source preview uses Mesa 412 OS.

Exact gain, master, bass, middle, treble, presence and switch positions are **not supplied for these individual snapshots**. Do not infer numeric knob positions from their names. All five declare `tone_type: overdrive`. The page describes Ford settings as generally Rock mode, but that is not an exact per-file settings record.

## Selected models and exact paths

The labels below abbreviate the original filenames. Intended character is inferred from uploader names, not an audio assessment.

| Label | Selection rationale | Embedded gain metric* | Reported validation ESR | Needs cab IR |
| --- | --- | ---: | ---: | --- |
| Smooth | First candidate for smooth lead | 0.831549 | 0.00419022 | Yes |
| Bluesy | First candidate for blues lead | 0.840310 | 0.00527200 | Yes |
| Open Robben | Alternative open lead voicing | 0.851698 | 0.00760864 | Yes |
| Open Larry | Alternative open lead voicing | 0.804648 | 0.00408194 | Yes |
| Carlosanta | Alternative singing-lead candidate | 0.844233 | 0.00899149 | Yes |

*`metadata.gain` is stored model metadata, **not** the physical amp gain-knob position or a recommended input-gain multiplier.

1. `D:\Amp Modeler\resources\lead-models\SLAMMIN_DUMBLE_FORD_OD_SMOOTH_S.nam`
2. `D:\Amp Modeler\resources\lead-models\SLAMMIN_DUMBLE_FORD_OD_BLUESY_S.nam`
3. `D:\Amp Modeler\resources\lead-models\SLAMMIN_DUMBLE_FORD_OD_OPEN_ROBBEN_S.nam`
4. `D:\Amp Modeler\resources\lead-models\SLAMMIN_DUMBLE_FORD_OD_OPEN_LARRY_S.nam`
5. `D:\Amp Modeler\resources\lead-models\SLAMMIN_DUMBLE_FORD_OD_CARLOSANTA_S.nam`

## Validation and compatibility

All five passed local JSON parsing and checks for version `0.5.4`, architecture `WaveNet`, sample rate `48000`, two layer arrays and exactly **13,802 finite numeric weights**. They are conventional A1 Standard models, with 16/8 channels, kernel size 3, Tanh activation, no gating, and dilation sequence 1/2/4/8/16/32/64/128/256/512 in each layer array. Head scale is 0.02. No A2 container or custom architecture is required.

Embedded metadata identifies `modeled_by: slamminmofo`, `gear_type: amp`, model names matching the filenames and model dates of 2025-06-18. The embedded make/model fields are placeholders (`tz-make` / `tz-model`); the amp identity above comes from the original upload description. Both input and output calibration metadata are **12.2418 dBu**, while the source description rounds/describes its chain as +12.4 dBu. Preserve the file values for loaders that support calibration.

**Capture-quality caveat:** all five contain `training.data.checks.passed: false` and `training.settings.ignore_checks: true`. The source says it used the TTSv10 signal. The exact reason for the failed checks is not established here. Listed ESR values are uploader/trainer metadata, not independently reproduced measurements.

Follow-up integration: all five loaded and rendered successfully through the application's actual NAM/IR audition chain, with finite output and successful PCM24 readback. Reports and WAVs are in `docs/audition-dumble-*`. This is technical validation, not a listening evaluation or a guarantee of capture quality. The personal `resources/lead-models/catalog.json` adds all five to the amp selector under Dumble-style / ODS #102 Clone / Lead. An appropriate cabinet IR is selected with each configuration.

## Integrity

SHA-256 values calculated from the extracted original bytes:

| Label | Bytes | SHA-256 |
| --- | ---: | --- |
| Smooth | 284122 | `0BD18893F1521B1723A4099F7A160FD039E45B15B99D5970983316F774EDD4D2` |
| Bluesy | 284514 | `91BFB8BA3873BADE8D016BD61041714CF2991974FA52EE67846C05F0830E0585` |
| Open Robben | 284027 | `2C90129A76FDD0A8C26C0A155988AA8870814FDA13A4A6FFD411A055E4AFDA1B` |
| Open Larry | 284772 | `83EDEF0459A2B43EAF979D74391F92BBB12A35FF2DE1624B8B56991017C39B48` |
| Carlosanta | 283996 | `7B7D2647248422338BF5C59181A63D1FDC4495D2DD6F57B9B78202CE9DFFA97E` |
