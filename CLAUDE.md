# CLAUDE.md — working rules for this repo

## The developer
- Tom works on **Windows** with VS Code + PlatformIO. Never give Linux/bash
  commands in instructions for him; use the PlatformIO GUI or PowerShell.
- If a Python tool is ever needed on his side: always a venv, and
  `python -m pip install`, never bare `pip install`.
- He does not want to repeat instructions. Anything decided goes in this file,
  `docs/SPEC.md`, or `THIRD_PARTY_NOTICES.md`.

## Board naming (Tom's rule)
- Name boards by what a user can identify: **screen size, display driver,
  touch type**. Never by an information site (LCDwiki is a datasheet
  resource, not a maker or seller) and not by vendor model codes.
- Firmware files: `CYD_<size>in_<DRIVER>_<Resistive|Capacitive>.bin`,
  e.g. `CYD_3.2in_ST7789_Resistive.bin`. No version in the file name (the
  release tag carries it), so links to the latest release stay stable.
- PlatformIO env names can't contain dots, so envs are short
  (`cyd32_st7789_res`) and carry `custom_firmware_name`,
  `custom_board_title` and `custom_board_hint`. CI, releases and the web
  flasher all read those - platformio.ini is the single source of truth.
  Adding a board = new env with those three options + board_select.h entry.

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

## Code layout and checks
- `src/game/` sudoku core + rules: plain C++, no Arduino/LVGL. Tested on the
  PC by `tools/host_tests/` (CI runs them with sanitizers before building).
- `src/ui/` LVGL screens; hardware actions go through `UiHooks`, so the UI
  also builds in `tools/preview/` (Linux-only helper Claude uses to render
  screenshots at 240x320 and 320x480 before shipping UI changes).
- `src/app/save_store.*` saves the game to LittleFS (`/game.bin`, written
  via temp file + rename).
- Layout is portrait for now; Tom will decide after seeing it on hardware.
- UI rules from Tom: strong highlight tints with distinct hues (cheap TN
  panels wash out pale tints at an angle); no shrinking fonts to squeeze
  labels in; menu is a 3-line hamburger in the top bar; colors come from
  `src/ui/theme.cpp` palettes (Light/Dark), never hard-coded elsewhere.
- Settings the player picks (theme, input mode) are saved by
  `src/app/settings_store.*` (`/ui_settings.bin`). Digit 1st is the default.
- Stats: `src/game/stats.*` (CSV format + summary, host-tested) and
  `src/app/stats_store.*` (SD `/CYD-Sudoku/stats.csv` if the board's SD is
  usable and a card is in, else LittleFS `/stats.csv`, newest 250). SD access
  is `src/hal/sdcard.*`, gated by `BOARD_SD_USABLE` in the board file.
- Memory: LVGL allocates from the ESP32 heap (LV_STDLIB_CLIB). A fixed
  LV_MEM_SIZE pool overflowed static DRAM once it needed > 64 KB. The PC
  preview uses a fixed pool (-DCYD_PREVIEW) and prints per-screen usage -
  check it when adding screens, and keep tables as one label per column
  rather than objects per cell.
- Game saves are format 'SUD2' (adds hints); 'SUD1' still loads.
- Game clock: counts only while the game screen is up AND there was a touch
  in the last 2 minutes (kIdlePauseMs, via lv_display_get_inactive_time).
  Times feed the stats, so don't add anything that counts unattended time.

## Known hardware issues
- Tom's boards: 2.8" ESP32-2432S028 in both ILI9341 and ST7789 versions
  (src/boards/esp32_2432s028.hpp), and "ESP32-32E" display boards 3.2"
  ST7789, 3.5" ST7796, 4.0" ST7796, all resistive
  (src/boards/esp32_32e_display.hpp). The ESP32-32E boards have no model
  number on the PCB - only text like "3.2" LCD Display, ESP32-32E, 240x320,
  Resistive Touch". They are NOT ESP32-3248S0xx boards; never use that
  pinout for them. Their datasheets are on lcdwiki.com.
- 2.8" ESP32-2432S028: touch (XPT2046) uses VSPI on pins 25/32/39/33; the SD card
  is also VSPI but on 18/19/23/5. LovyanGFX's XPT2046 driver has no software
  SPI, so SD on this board needs a bit-banged XPT2046 touch class or bus
  switching. LittleFS is used first for this reason.
- ESP32-32E boards: touch shares the display's HSPI bus (CS 33); SD is on
  VSPI by itself, so SD works there (BOARD_SD_USABLE 1).
- 2.8" SD (BOARD_SD_USABLE 0) needs a bit-banged XPT2046 touch driver first;
  until then stats on the 2.8" go to LittleFS.
- Tom uses a Nintendo DS Lite stylus with firm presses - NOT a finger.
  Don't explain touch problems with finger size or light pressure.
- 4.0" first-tap offset (first tap after idle landed ~1 cell low or right):
  FIXED in v0.1.0-alpha.4, confirmed by Tom. The fix: a press starts only
  after two consecutive readings agree within 8 px, ends after two empty
  readings; ESP32-32E touch clock 1 MHz. Keep both. Diagnostic still
  available: Display & touch > Touch test; `-D CYD_TOUCH_DEBUG` logs raw
  touches to serial.
- Panel inversion / red-blue order differ between production runs. They are
  fixed per unit on the device (src/hal/panel_prefs.*, saved to LittleFS),
  not with build flags.

## Licensing
MIT. Only copy code from MIT/BSD/Apache/zlib/public-domain sources and keep
their headers. GPL projects (QQwing, OpenSudoku, LibreSudoku) are reference
only. Update `THIRD_PARTY_NOTICES.md` whenever code is brought in.

## Releases and web flasher
- Every push to main: CI builds every env that has `custom_firmware_name`
  and redeploys the web flasher (GitHub Pages,
  https://tomtombombadil.github.io/CYD-Sudoku/) with that build
  (version `dev-<sha>`).
- Releases: Actions tab -> Build -> "Run workflow" on main with
  `release_tag` = `vX.Y.Z` (hyphen = pre-release). The workflow creates the
  tag and a GitHub release with the `.bin` files (merged factory images,
  flash at 0x0). Claude's sessions cannot push tags (proxy returns 403), so
  Claude releases via this dispatch, not `git push --tags`. Pushing a `v*`
  tag from Tom's machine also works.
- Site source: `web/index.html`; assembled by `tools/make_site.py`.
- Only give an env `custom_firmware_name` once its board file exists.
