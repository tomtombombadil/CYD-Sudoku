# Third-party components and sources

CYD Sudoku is MIT licensed. Everything it builds on, or plans to borrow from,
is listed here with its license. **Rule: only MIT, BSD, Apache-2.0, zlib or
public-domain code may be copied into this repository.** GPL projects can be
studied for UX ideas but no code from them may be copied.

## Libraries (linked, pulled in by PlatformIO)

| Component | Use | License |
|---|---|---|
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Display, touch, backlight drivers | MIT and BSD-2-Clause |
| [LVGL](https://github.com/lvgl/lvgl) 9.x | UI widgets and rendering | MIT |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) via [pioarduino](https://github.com/pioarduino/platform-espressif32) | Framework / core | LGPL-2.1 (core), Apache-2.0 (ESP-IDF) |

The Arduino core is LGPL-2.1. Because this project's full source is public,
anyone can rebuild the firmware against a modified core, which satisfies the
LGPL for the binaries published in Releases.

## Reference material

| Source | What we take | License |
|---|---|---|
| [witnessmenow/ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) | Pinouts, panel settings | MIT |

## Planned Sudoku sources (not yet copied in)

| Source | Planned use | License | OK to copy code? |
|---|---|---|---|
| Simon Tatham's Portable Puzzle Collection — `solo.c` | Generator + difficulty grading by human solving techniques | MIT | Yes |
| [t-dillon/tdoku](https://github.com/t-dillon/tdoku) | Fast solver / uniqueness checks; algorithm notes | BSD-2-Clause | Yes (keep notice) |
| [grantm/sudoku-exchange-puzzle-bank](https://github.com/grantm/sudoku-exchange-puzzle-bank) | Fallback puzzle packs on LittleFS (easy / medium / hard / diabolical) | Public domain (Unlicense-style) | Yes |
| [stephenostermiller/qqwing](https://github.com/stephenostermiller/qqwing) | — | GPL-2.0 | **No** — reference only |
| OpenSudoku, [LibreSudoku](https://github.com/kaajjo/LibreSudoku) (Android) | UX ideas only | GPL-3.0 | **No** — reference only |

When code is copied in, keep its original copyright header in the file and
move its row to a "Included code" section here.
