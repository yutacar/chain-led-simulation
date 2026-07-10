# Third-Party Licenses

This project's own code is MIT-licensed (see [`LICENSE`](LICENSE)). It builds on and links against
the third-party components listed below, each under its own license.

> **Packaging status**: Phase 8 (packaging) is complete — see [`PLAN.md`](PLAN.md) /
> [`NOTES.md`](NOTES.md) "Phase 8". A real `.deb` (`chainled_1.0.0_arm64.deb`) has been built via the
> CardputerZero cross-build, `prepublish_check.py` passes with zero errors/warnings, and a
> clean-room `apt install`/`apt remove` round trip has been verified in a fresh container. The
> table below describes what each dependency is used for and how it's pulled in by the build
> (fetched from source, resolved from the system package manager at build time, or listed in the
> `.deb`'s own `Depends:` field — see `cmake/cm0-package.cmake`'s `CPACK_DEBIAN_PACKAGE_DEPENDS`,
> hand-verified against the real cross-built binary's `readelf -d` output). Hub AppStore submission
> via `czdev publish` is done — [CardputerZero/packages#62](https://github.com/CardputerZero/packages/pull/62)
> is open. Phase 9's remaining item, real-hardware validation, is still pending — see `NOTES.md`
> "Phase 9" for the checklist.

| Component | License | Used for | How it's pulled in |
| --- | --- | --- | --- |
| [LVGL](https://github.com/lvgl/lvgl) v9.5.0 | MIT | Core GUI framework — all screens, widgets, rendering | Fetched from source (`CMakeLists.txt` `FetchContent`), statically linked |
| Noto Sans JP / Noto Sans SC (subset) | SIL OFL 1.1 | CJK glyph rendering for the Japanese and Simplified Chinese UI, subsetted at build time by [`tools/generate_cjk_fonts.py`](tools/generate_cjk_fonts.py) to only the glyphs actually used (~32KB each) | Generated into `assets/fonts/NotoSans{JP,SC}-Subset.otf`, loaded at runtime by `i18n/fonts.cpp` |
| [fmt](https://github.com/fmtlib/fmt) | MIT | Logging and formatted strings | System package manager (desktop); fetched from source as a fallback on the CM0 cross-build if not found in the BSP sysroot |
| [GoogleTest](https://github.com/google/googletest) v1.15.2 | BSD-3-Clause | Unit/headless test suite (`tests/`) only; not part of the app itself | Fetched from source (`FetchContent`), desktop builds only |
| [SDL2](https://www.libsdl.org/) | zlib | Desktop simulator display/input backend (macOS/Linux/Windows development builds only) | System package manager; desktop-only, the CardputerZero device build does not use SDL at all |
| [FreeType](https://freetype.org/) | FTL or GPLv2 (dual-licensed; this project uses the FTL option, compatible with MIT distribution) | Runtime TTF/OTF font rasterization (Latin + CJK) | System package manager |
| libpng, libjpeg-turbo, zlib | libpng license, IJG/BSD-style, zlib | Image decoding used by LVGL (PNG/JPEG assets) and general compression | System package manager |
| [BlueZ](http://www.bluez.org/) (via `libsystemd`'s `sd-bus`, D-Bus) | Accessed only via the system D-Bus API; not vendored or linked as source | `BleTransport` (`src/net/ble/`) talks to the system `bluetoothd` over D-Bus for BLE animation distribution (real-device default, gated behind the `CHAINLED_BLE` CMake option — Linux-only, never compiled on the macOS desktop build) | System service + system `libsystemd` package |

## Notes

- **Template origin**: this repository began as a fork of the CardputerZero official template
  (https://github.com/CardputerZero/template), which is itself MIT-licensed
  (`SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD`, `SPDX-License-Identifier: MIT`
  headers preserved unmodified throughout the original files). See [`LICENSE`](LICENSE).
- **`assets/fonts/LICENSE.txt`** now documents Noto Sans JP/SC (OFL-1.1) as entry 0, added as part
  of Phase 8 since this file is what actually ships in the `.deb`
  (`cmake/cm0-package.cmake` installs it alongside `assets/fonts/NotoSans*.otf`, and again as
  `third-party-assets-license.txt` under the package's doc directory). It also still documents the
  Inter font family (OFL-1.1), Kenney input-prompt icons (CC0), and Phosphor Icons (MIT), all
  inherited from the upstream template and unused by this app's own UI (Title/Editor/Simulator/
  Send/Receive/Settings all use LVGL's builtin Montserrat bitmap font plus the Noto Sans CJK
  subsets, not Inter or Phosphor — see [`NOTES.md`](NOTES.md) "Phase 6") — those entries are kept
  for provenance but are inherited-but-unused, same as Battleship's own
  `THIRD_PARTY_LICENSES.md` documents for its own equivalent leftovers.
- **Chain Mono/RGB protocol**: the UART daisy-chain protocol implemented in `src/chain/` was
  reverse-engineered from the official [m5stack/M5Chain](https://github.com/m5stack/M5Chain)
  Arduino library's source (MIT-licensed) rather than copied from it — no code from that
  repository is vendored here. See [`NOTES.md`](NOTES.md) and the memory reference for the exact
  protocol details this was derived from.
