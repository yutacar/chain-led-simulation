# Chain LED Simulation for CardputerZero

A dot-animation editor and LED-matrix simulator built for the [M5Stack CardputerZero](https://cardputerzero.github.io/) (320×170, LVGL v9.5 + C++/CMake).

- **Create**: draw 8×8 pixel animations (up to 10 frames) with a keyboard-only editor, per-frame duration control, flip/duplicate/undo-style frame ops.
- **Auto-generate**: draw one frame and let `ProceduralCompleter` fill in the rest with deterministic pattern transforms (a local-LLM completer was evaluated and rejected on RAM grounds — see [Architecture](#architecture)).
- **Simulate & drive real hardware**: preview on-screen in RGB or Mono mode, then output the same animation live to a real Chain Mono / Chain RGB (M5Stack Chain series) 8×8 LED matrix over UART.
- **Distribute**: send/receive animations between two CardputerZero units over **BLE** (default, real-device path) or **TCP/LAN** (desktop development path).
- **Languages**: English, 日本語 (Japanese), 简体中文 (Simplified Chinese) — switchable anytime from Settings, no restart required.

日本語版の README は [README.ja.md](README.ja.md) を参照してください。

## Screenshots

| Title | Editor | Simulator |
| --- | --- | --- |
| ![Title](docs/screenshots/title_en.png) | ![Editor](docs/screenshots/editor_en.png) | ![Simulator](docs/screenshots/simulator_en.png) |

| Send / Receive | Settings | Real Chain output |
| --- | --- | --- |
| ![Send/Receive](docs/screenshots/send_receive_en.png) | ![Settings](docs/screenshots/settings_en.png) | ![Real Chain output](docs/screenshots/chain_sim_real_toggle.png) |

More screenshots, including the Japanese and Simplified Chinese UI, are in [`docs/screenshots/`](docs/screenshots/) (see the `*_ja.png` / `*_zh.png` files).

## Installation

**From the CardputerZero Hub AppStore**: not yet listed — see [Status](#status) below.

**Manual install**, once you've built a `.deb` yourself (see [Development](#development)):

```shell
sudo dpkg -i chainled_1.0.0_arm64.deb
# or, to also pull in any missing dependencies:
sudo apt install --no-install-recommends ./chainled_1.0.0_arm64.deb
```

The app appears in APPLaunch after installation.

### Notes for running on real hardware

- **BLE (default distribution mode)** requires the device's Bluetooth stack (BlueZ) to be running and the app's user account to be in the `bluetooth` group (`sudo usermod -aG bluetooth <user>`, then re-login). Without this, BLE features fail to start rather than crashing.
- **TCP/LAN mode** is a desktop development path (fixed port, `127.0.0.1` only) — it isn't intended as a real-device distribution method.
- Real Chain Mono/RGB output uses the Grove UART port; set `CHAINLED_UART_DEVICE` if the serial device isn't at the default path.

## Key Controls

| Screen | Key | Action |
| --- | --- | --- |
| Title | Arrow keys / Enter | Move focus / select a menu entry |
| Title | Esc (hold, ~1s) or Home | Quit the app safely from anywhere |
| Editor | Arrow keys / Enter | Move the pixel cursor / toggle the pixel under it |
| Editor | `C` | Cycle the draw color |
| Editor | `F` / `V` | Flip the current frame horizontally / vertically |
| Editor | `N` / `D` / Backspace | New frame / duplicate current frame / remove current frame |
| Editor | `,` / `.` | Previous / next frame |
| Editor | `+` / `-` | Adjust the current frame's display duration |
| Editor | `G` | Generate the remaining frames from this one (`ProceduralCompleter`) |
| Editor | `S` / `X` | Save the animation / delete the saved file |
| Editor | `P` | Preview in the Simulator |
| Simulator | `M` | Toggle RGB / Mono preview |
| Simulator | `R` | Toggle live output to a real Chain Mono/RGB unit |
| Any screen | Esc (short press) | Go back one screen |

## Development

### Build (macOS desktop)

Install dependencies with Homebrew:

```shell
brew install cmake ninja libpng jpeg fmt sdl2 freetype dpkg zlib
```

Configure, build, and test (Apple Silicon):

```shell
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64-dbg
ctest --preset darwin-arm64-dbg
```

Run the desktop simulator:

```shell
./build/darwin-arm64/Debug/chainled
```

For Intel Macs, use the `darwin-x86-64` presets instead. See [`NOTES.md`](NOTES.md) for the CardputerZero cross-build/device build (including the Docker `debian:trixie` device-build runbook), CMake options (`CHAINLED_BLE`, `CHAINLED_WINDOW_ZOOM`, etc.), and the full phase-by-phase development history. [`PLAN.md`](PLAN.md) has the original design rationale.

### Status

This project is under active development (see [`PLAN.md`](PLAN.md) for the full phase plan). Phases 0–8 are complete: animation core, editor/simulator UI, procedural frame generation (with a local-LLM path evaluated and rejected — RAM budget, see `NOTES.md` "Phase 3: Go/No-Go"), Chain Mono/RGB protocol + device output, TCP and BLE distribution, EN/JA/ZH localization, and packaging — a real `.deb` builds cleanly from the CardputerZero cross-build, [`prepublish_check.py`](https://github.com/CardputerZero/skill) passes with zero errors/warnings, and a clean-room `apt install`/`apt remove` round trip has been verified in a fresh container.

**Not yet done**: real-hardware validation and the CardputerZero Hub AppStore submission (`czdev publish`) — both require physical CardputerZero + Chain Mono/RGB hardware and the maintainer's own GitHub OAuth login, neither of which are available in the environment this project has been developed in so far. See [`NOTES.md`](NOTES.md) "Phase 9" for the exact pending checklist. Until that's done, there's no Hub AppStore listing — building your own `.deb` from source (above) is the only way to install it.

### Architecture

The codebase separates animation logic, device output, networking, and UI so the whole app can be developed and tested on desktop before ever touching real hardware:

```
view/        LVGL v9 screens, widgets, pixel-grid rendering, key input handling
i18n/        String catalog (EN/JA/ZH) + CJK font loading, no LVGL/animation-logic dependency
core/        Pure C++ animation logic — 8x8 frame model, (de)serialization, playback timing.
             No I/O, no LVGL, no threads.
generator/   AnimationGenerator: ProceduralCompleter (deterministic frame completion from a
             single seed frame) -- an LlmCompleter was evaluated and rejected, see NOTES.md
chain/       Chain Mono/RGB UART protocol (encode/decode) + RealChainOutput (POSIX termios)
net/         Transport-agnostic protocol (framing, ACK/retry) with TcpTransport (desktop/LAN
             dev path) and BleTransport (BlueZ/D-Bus) implementations behind one ITransport
             interface
```

`core/`, `chain/`, `generator/`, and `net/` have no LVGL dependency and are covered by an extensive `ctest` suite (headless, including a lossy/duplicating/fragmenting network soak test and real-socket transfer tests). See [`PLAN.md`](PLAN.md) for the original design rationale and [`NOTES.md`](NOTES.md) for what actually got built, including known limitations.

## License

This project is released under the [MIT License](LICENSE). Third-party components (LVGL, fonts, fmt, etc.) are under their own licenses — see [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the full list.
