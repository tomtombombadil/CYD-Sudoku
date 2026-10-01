# CLAUDE.md — working rules for this repo

## The developer
- Tom works on **Windows** with VS Code + PlatformIO. Never give Linux/bash
  commands in instructions for him; use the PlatformIO GUI or PowerShell.
- If a Python tool is ever needed on his side: always a venv, and
  `python -m pip install`, never bare `pip install`.
- He does not want to repeat instructions. Anything decided goes in this file,
  `docs/SPEC.md`, or `THIRD_PARTY_NOTICES.md`.

## Architecture
- One PlatformIO env per board; each sets exactly one `CYD_BOARD_*` flag.
- `src/boards/board_select.h` maps the flag to a board file providing
  `class LGFX`, `BOARD_NAME`, `BOARD_TOUCH_RESISTIVE`, `BOARD_PIN_BOOT_BTN`.
  No other file may test `CYD_BOARD_*` flags.
- LVGL only talks to hardware through `src/hal/lvgl_port.cpp`.
  LovyanGFX applies touch calibration + rotation; LVGL rotation stays 0.
- Layout must derive from `lv_display_get_horizontal/vertical_resolution()`,
  never hard-coded 320x240.
- Resistive-touch UX: big targets, no swipes/gestures.

## Known hardware issues
- ESP32-2432S028R: touch (XPT2046) uses VSPI on pins 25/32/39/33; the SD card
  is also VSPI but on 18/19/23/5. LovyanGFX's XPT2046 driver has no software
  SPI, so SD support will need either a small bit-banged XPT2046 touch class
  or careful bus switching. LittleFS is used first for this reason.
- Panel color order / inversion differ between production runs; overridable
  with `-D CYD_PANEL_RGB_ORDER=1` / `-D CYD_PANEL_INVERT=1`.

## Licensing
MIT. Only copy code from MIT/BSD/Apache/zlib/public-domain sources and keep
their headers. GPL projects (QQwing, OpenSudoku, LibreSudoku) are reference
only. Update `THIRD_PARTY_NOTICES.md` whenever code is brought in.

## Releases
Tag `vX.Y.Z` on main → GitHub Actions builds every implemented env and
attaches `cyd-sudoku-<env>-vX.Y.Z.bin` (merged factory image, flash at 0x0).
Add an env to the workflow matrix only once its board file exists.
