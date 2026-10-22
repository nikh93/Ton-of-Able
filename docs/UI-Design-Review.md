# UI redesign

Implemented from UI Design überarbeiten.txt:

- Walnut sound panel and darker wood transport panel, procedural grain, inset glass meters, gradient glass preset/effect surfaces, raised buttons and rotary controls.
- Bounded content size, fixed-size main knobs, separate sound / preset / source / effect groups.
- Removed duplicate main Save and A/B controls; preset component retains Save, Save As, Load, Rename, Delete, A/B.
- Effect bypass controls live in the effect rack. Fixed the layout loop that hid expanded parameters.
- NAM/IR import, library management, settings, diagnostics, source switching, WAV transport and recording remain accessible.
- MSVC /utf-8 guarantees the encoding of source literals.

Verification: Release build and existing CTest suite passed. Native Windows screenshot capture failed twice with SetIsBorderRequired / E_NOINTERFACE. Actual JUCE component snapshots were exported instead, from the running application at widths 1020, 1280 and 1800, for all four expanded effects and collapsed state. These are rendering checks, not an OS DPI or audio hardware certification.

The initial screenshots were reviewed and the large-window content width and A/B highlight adjusted. Final captures are in design-final. The installed executable supports --design-preview="absolute output directory" for repeating these rendering checks.
