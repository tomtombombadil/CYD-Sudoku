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
- Chip first when it isn't the plain ESP32, the maker's model in
  parentheses at the end (Tom's rule from CYD-Classic-Games, 2026-10-16):
  `ESP32-S3 4.0" ST7796 Capacitive (Freenove FNK0104S)` ->
  `CYD_ESP32-S3_4.0in_ST7796_Capacitive.bin`. The flasher reads
  `custom_chip_family` (env or the section it extends; default ESP32) for
  the manifest's chipFamily.
- Web flasher: one dropdown + one Install button (Tom, 2026-11-08), then a
  "Which board do I have?" reference list whose rows pick in the dropdown.
  `?board=<firmware name>` preselects; the last choice is remembered.

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
  README screenshots: `docs/screenshots/`, made from preview renders by
  `tools/preview/readme_shots.py`. Re-render them when a shown screen
  changes.
- `src/app/save_store.*` saves the game to LittleFS (`/game.bin`, written
  via temp file + rename).
- Layout is portrait - final (Tom: comfortable one-handed). Don't add
  landscape.
- No "tap again" / "are you sure" confirmations, ever. Buttons act on the
  first tap.
- UI rules from Tom: strong highlight tints with distinct hues (cheap TN
  panels wash out pale tints at an angle); no shrinking fonts to squeeze
  labels in; menu is a 3-line hamburger in the top bar; colors come from
  `src/ui/theme.cpp` palettes (Light/Dark), never hard-coded elsewhere.
- Difficulty = hardest technique needed, from `src/game/grader.*` (L1
  singles, L2 locked candidates, L3 pairs/triples/X-Wing, L4
  Swordfish/XY/XYZ-Wing). `generate()` must return a puzzle that grades
  exactly its level; host tests enforce it. Never go back to clue counts.
  Calibration tool: `tools/grader_check/` (Sudoku Exchange bank).
- `src/app/puzzle_stock.*` pre-generates 2 puzzles per level on a core-0
  idle-priority task (`/puzzle_stock.bin`); UI gets them via
  `UiHooks::take_puzzle`.
- Solve flash toggles the panel invert bit (`UiHooks::flash_invert`), no
  overlay animations. Brightness is a slider in Display & touch, saved in
  UiSettings (floor kMinBrightness).
- Settings the player picks (theme, input mode, brightness) are saved by
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
- Restart: an unsolved puzzle keeps its clock and hint count; a solved one
  restarts as a replay (`Game::replay()`, spare bit 87 of the hint bits)
  whose solve isn't recorded again. Same as CYD-Classic-Games.
- CYD-Classic-Games (tomtombombadil/CYD-Classic-Games) carries a port of
  this Sudoku (src/games/sudoku/). Fixes flow both ways: when one changes
  the Sudoku core, grader, rules or stats store, check the other.
- Game clock: counts only while the game screen is up AND there was a touch
  in the last 2 minutes (kIdlePauseMs, via lv_display_get_inactive_time).
  Times feed the stats, so don't add anything that counts unattended time.

## Known hardware issues
- Supported boards: 2.8" ESP32-2432S028 in ILI9341 and ST7789 versions
  (src/boards/esp32_2432s028.hpp), "ESP32-32E" display boards 3.2"
  ST7789, 3.5" ST7796, 4.0" ST7796, all resistive
  (src/boards/esp32_32e_display.hpp), the Freenove FNK0104S ESP32-S3 4.0"
  (src/boards/freenove_s3_display.hpp) and the NM-CYD-C5
  (src/boards/nm_cyd_c5.hpp). Tom owns and has tested the five classic
  boards (the 2.8" ILI9341 and 3.5" via CYD-Classic-Games, same board
  files). The S3 and C5 carry `custom_board_tested = no`.
- The ESP32-32E boards have no model number on the PCB - only text like "3.2" LCD Display, ESP32-32E, 240x320,
  Resistive Touch". They are NOT ESP32-3248S0xx boards; never use that
  pinout for them. Their datasheets are on lcdwiki.com.
- 2.8" ESP32-2432S028: touch (XPT2046) uses VSPI on pins 25/32/39/33; the SD card
  is also VSPI but on 18/19/23/5. LovyanGFX's XPT2046 driver has no software
  SPI, so SD on this board needs a bit-banged XPT2046 touch class or bus
  switching. LittleFS is used first for this reason.
- Sound on the 2.8" ILI9341 (not the ST7789): its amplifier only runs
  while GPIO 4 (also the red LED, active low) is LOW. board_select.h
  defines BOARD_PIN_AUDIO_EN 4 in the ILI9341 branch only - never in the
  shared esp32_2432s028.hpp; the ST7789's sound works without it (Tom).
  Boot keeps it HIGH (off), after the LED setup. Any future sound must
  switch the amp on early (~100 ms wake-up) and keep it on, or short
  sounds are clipped. Red LED glows while it's on - expected. Source:
  CYD-GoldBoxGameEngine v0.179.2; witnessmenow/ESP32-Cheap-Yellow-Display
  issue #372.
- ESP32-32E boards: touch shares the display's HSPI bus (CS 33); SD is on
  VSPI by itself, so SD works there (BOARD_SD_USABLE 1).
- 2.8" SD (BOARD_SD_USABLE 0) needs a bit-banged XPT2046 touch driver first;
  until then stats on the 2.8" go to LittleFS.
- `custom_board_tested = no` shows as "untested" on the flasher and README.
  Remove it only when someone reports the board working.
- ESP32-S3 boards (`[esp32_s3]`: 16 MB DIO flash, 8 MB octal PSRAM,
  `partitions_16mb.csv` = 6 MB app + 2 MB LittleFS, native USB CDC with TX
  timeout 0 so an unread port never stalls). Freenove FNK0104S: ST7796 on
  SPI2 at 80 MHz, invert on, BGR; FT6336U capacitive touch (no calibration;
  `BOARD_TOUCH_RESISTIVE 0` drops Recalibrate); SD on SDMMC 4-bit
  (`BOARD_SD_MMC`); amp enable GPIO 1 kept HIGH (no sound). Same board file
  as CYD-Classic-Games minus its ES8311 sound.
- ESP32-C5 (`[esp32_c5]`: 16 MB flash, same partitions, single RISC-V core,
  needs LovyanGFX >= 1.3.0). NM-CYD-C5 pins from RockBase's TFT_eSPI setup
  and schematic (github.com/Yverr0y/NM-CYD-C5): one SPI bus (SCLK 6, MOSI
  7, MISO 2) for display (CS 23, DC 24, BL 25), XPT2046 touch (CS 1, no IRQ)
  and SD (CS 10); BOOT = GPIO 28; speaker in GPIO 26 (no amp enable);
  WS2812 on 27. Inversion off, BGR. SD is off (BOARD_SD_USABLE 0) until
  bus sharing with LovyanGFX is written and tested. Serial log via the
  CH340 port (UART0).
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
- Every push to main: CI runs the tests, builds every env that has
  `custom_firmware_name`, and redeploys the web flasher (GitHub Pages,
  https://tomtombombadil.github.io/CYD-Sudoku/) with that build (version
  `dev-<sha>`). This is how Tom gets builds for testing - keep it that way.
- Don't publish a release unless Tom asks for one. v1.0.0 is out
  (2026-10-01); later releases follow semver from there.
- Releases: Actions tab -> Build -> "Run workflow" on main with
  `release_tag` = `vX.Y.Z` (hyphen = pre-release). The workflow creates the
  tag and a GitHub release with the `.bin` files (merged factory images,
  flash at 0x0). Claude's sessions cannot push tags (proxy returns 403), so
  Claude releases via this dispatch, not `git push --tags`. Pushing a `v*`
  tag from Tom's machine also works.
- Site source: `web/index.html`; assembled by `tools/make_site.py`.
- Only give an env `custom_firmware_name` once its board file exists.
