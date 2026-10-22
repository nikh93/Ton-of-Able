# Plugin downloads and installation

Checked on 2026-09-15. Personal-use installation authorized by the user. Preferred VST3 scan directory: `D:\Amp Modeler\plugins`. Download/cache directory: `D:\Amp Modeler\downloads`.

## BYOD — installed

- Version: **1.3.0**, confirmed in the installed binary's FileVersion and ProductVersion resources.
- Format/platform: VST3, Windows x64; PE machine `0x8664` (AMD64).
- Host scan path (bundle): `D:\Amp Modeler\plugins\BYOD.vst3`
- Exact binary path: `D:\Amp Modeler\plugins\BYOD.vst3\Contents\x86_64-win\BYOD.vst3`
- Official product page: https://chowdsp.com/products.html
- Official release: https://github.com/Chowdhury-DSP/BYOD/releases/tag/v1.3.0
- Official installer URL: https://github.com/Chowdhury-DSP/BYOD/releases/download/v1.3.0/BYOD-Win-64bit-1.3.0.exe
- Cached installer: `D:\Amp Modeler\downloads\BYOD-Win-64bit-1.3.0.exe`
- Installer SHA-256: `A53FB65CEDF90F7673E21AC2D12F5B0778F003AE7F2E34E6C4301854A738C36C`
- Installed binary SHA-256: `572BAAF900D52603B6F24503218107C3A60DD056F7D1BA87D08B6DAF33A9603A`

### Installation and verification

The official release provides a Windows installer, not a Windows plugin archive. Its published Inno Setup script uses custom destination pages without a command-line VST3 destination override:
https://github.com/Chowdhury-DSP/BYOD/blob/v1.3.0/installers/windows/BYOD_Install_Script.iss

Extracted the official installer with innoextract 1.9, then copied the complete `BYOD.vst3` bundle to the preferred directory. Full extraction retained in `D:\Amp Modeler\downloads\BYOD-1.3.0-extracted`. No installer registration or uninstallation was performed.

PowerShell Authenticode verification of the installer returned **Valid / Signature verified**, signer **Mostly Laminar Music LLC**, certificate thumbprint `BE63ACD6ED6922E74A726EAAD6374D29765C9499`. The extracted VST3 binary itself is **NotSigned**; its provenance is the verified signed installer. SHA-256 values above are locally calculated fingerprints, not vendor-published checksums.

Windows successfully loaded the installed x64 binary and resolved its `GetPluginFactory`, `InitDll`, and `ExitDll` exports. Follow-up host tests also verified audio processing at 44.1/48/96 kHz, mono/stereo routing, partial blocks, state and parameter restoration, missing-plugin retention and native-editor creation/destruction. Detailed results: `docs/byod-host-checks.xml`. The host enumerated 40 factory programs, including John Mayer, Clapton, King Of Tone and ZenDrive.

License retained at `D:\Amp Modeler\downloads\BYOD-1.3.0-LICENSE`, downloaded unchanged from https://raw.githubusercontent.com/Chowdhury-DSP/BYOD/v1.3.0/LICENSE . The original installer is also retained.

### Extraction utility

- Official utility page: https://constexpr.org/innoextract/
- Download: https://constexpr.org/innoextract/files/innoextract-1.9-windows.zip
- Cache: `D:\Amp Modeler\downloads\innoextract-1.9-windows.zip`
- Extracted utility and all bundled third-party license files: `D:\Amp Modeler\downloads\innoextract-1.9`
- Archive MD5 matched the checksum published on the utility's official page: `72d0d0dd874b6236eaa44411f4470ee1`. This is a download-integrity check; no OpenPGP signature verification was performed.

## Valhalla Supermassive — installed

- Installed Windows version: **5.0.0**, confirmed by ProductVersion and FileVersion; ProductName is `ValhallaSupermassive`.
- Official product page: https://valhalladsp.com/shop/reverb/valhalla-supermassive/
- Official download page: https://valhalladsp.com/demos-downloads/
- Final host scan / binary path: **`D:\Amp Modeler\plugins\ValhallaSupermassive.vst3`** (a single binary file, not a bundle directory).
- Architecture: Windows x64, PE machine `0x8664` (AMD64).
- User-supplied installer: `D:\Amp Modeler\downloads\ValhallaSupermassiveWin_V5_0_0.exe`, 3,670,888 bytes.
- Installer SHA-256: `18F934F1CF06E5ADD87CF33AA6249BA1932FE1C0FC960333659BDCCA93FC2DFF`.
- Installed VST3 SHA-256: `5C6FE8DBAB10A8FEE09C12244BB7C5AD6169E614EA558A0A456A8FBD5FD8F0AB`.
- Installer Authenticode: **Valid / Signature verified**, signer **Valhalla DSP, LLC**, certificate thumbprint `2EB773438072C5EA5420D274EFDFD5D34E4E3C47`, issuer Microsoft ID Verified CS EOC CA 01.
- Extracted VST3 Authenticode: **NotSigned**. Provenance is the verified signed installer. Hashes are locally computed fingerprints.

Extracted with the existing innoextract 1.9 into `D:\Amp Modeler\downloads\Supermassive-5.0.0-extracted`, then copied the packaged `code$GetVST3Dir\ValhallaSupermassive.vst3` binary unchanged to the final path. Windows successfully loaded it and resolved `GetPluginFactory`, `InitDll`, and `ExitDll`. Main host scanning/audio/editor tests are pending and are not implied by this loader check.

The original installer, including its embedded license material, is retained. All 204 factory `.vpreset` files are installed at `D:\Amp Modeler\plugins\ValhallaSupermassive-Presets\Factory`. The plugin-expected `C:\ProgramData\Valhalla DSP, LLC\ValhallaSupermassive\Presets\Factory` is a junction to that D: directory. The previously empty Factory folder was preserved as `Factory-empty-before-install`. All 204 files are reachable through the junction. Original extracted payloads remain in the downloads cache. No separate license file was present among extracted payloads. No VST2/AAX installation was performed.

The correct installer was supplied by the user after the automated download failures below. The precise direct CDN URL was not captured; the official product/download page URLs above are verified, and the supplied installer identity is verified by its valid vendor signature.

Direct HTTPS requests to the product/download pages (including the www hostname) returned HTTP 403 with a Cloudflare challenge. The in-app browser rendered the official download page and the Windows 5.0.0 button, but clicking it only displayed “Download in Progress”; no local Supermassive file was found in the inspected Downloads or Codex directories. The browser-exposed download form had empty download URL fields. An inferred vendor-storage Windows ZIP URL also returned HTTP 403 and was not accepted as a verified download source. No third-party plugin mirror was used.

`D:\Amp Modeler\downloads\valhalla-download-page.html` contains the failed HTTP challenge response, not a plugin or installer.

Download blocker resolved by the user-supplied, vendor-signed installer on 2026-09-15. Initial host checks passed 67 tests, with no errors; factory-program and non-dry-program checks were skipped because this plugin exposes zero host programs. Its presets are available in its own editor. See `docs/supermassive-host-checks.xml` for the detailed report and subsequent tail-test results.

## Scope

Final application-host report: `docs/byod-final-host-checks.xml`, 91 passes, 4 explicit King Of Tone diagnostic failures, 0 skips. John Mayer passed the full 18-format matrix, including 96 kHz. King Of Tone was silent at 96 kHz both through the slot and direct plugin processing, and remained silent after the tested return to 48 kHz. These failures are not suppressed or represented as a clean all-plugin certification.

User-supplied `downloads/ValhallaFutureVerbDemoWin_1_0_2/ValhallaFutureVerbDemoWin_1_0_2.exe` identifies itself as ValhallaFutureVerbDemo 1.0.2. It is not Supermassive; it was left uninstalled pending the user's choice.

No changes were made to `src` or CMake files by this installation task. Existing installations were not uninstalled. Host implementation work can scan `D:\Amp Modeler\plugins\BYOD.vst3` and `D:\Amp Modeler\plugins\ValhallaSupermassive.vst3`.
