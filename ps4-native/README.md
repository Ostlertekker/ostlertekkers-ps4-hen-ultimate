# Native PS4 port — implementation gates

This folder is deliberately a porting specification, not a fake native binary.

## First target
- Console firmware: 13.52
- GoldHEN: external dependency; never bundled, modified, or overwritten
- UI: PS5-inspired large-card layout
- Languages: German and English

## Why there is no `eboot.bin` or `.pkg` yet
A valid installable package requires:
- a compatible PS4 toolchain and linker,
- a native executable built against the selected SDK/toolchain,
- app metadata (`param.sfo`) and package layout,
- GP4/PKG generation,
- testing on the target firmware.

This environment does not contain the OpenOrbis SDK or a PS4 runtime, so claiming to have produced a tested PKG would be misleading.

## Porting layers
- `ui/`: render cards, focus states, navigation, localization.
- `platform/`: controller input, display, app lifecycle.
- `storage/`: app-owned settings and theme data.
- `compat/`: firmware detection and capability flags.
- `goldhen/`: read-only integration adapter, added only after interface verification.

## Acceptance checks
1. Launches as a standalone homebrew app after GoldHEN has been enabled.
2. D-pad navigation and X/O/back behavior are tested on-console.
3. Settings survive app restart.
4. German/English switch works everywhere.
5. Missing GoldHEN interface degrades gracefully.
6. Unknown firmware shows a compatibility warning rather than attempting unsupported calls.
7. Package installation does not touch GoldHEN or system firmware.
