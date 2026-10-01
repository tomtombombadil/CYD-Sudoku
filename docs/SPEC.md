# CYD Sudoku — Project Specification

## 1. Target hardware & constraints

**Boards** (one PlatformIO environment each):

| Firmware file | Env | Board identification | Panel | Touch | Hardware test |
|---|---|---|---|---|---|
| `CYD_2.8in_ILI9341_Resistive.bin` | `cyd28_ili9341_res` | Back: ESP32-2432S028(R) | ILI9341 240×320 | XPT2046, own pins | Untested (Tom has no unit) |
| `CYD_2.8in_ST7789_Resistive.bin` | `cyd28_st7789_res` | Back: ESP32-2432S028(R) | ST7789 240×320 | XPT2046, own pins | ✅ Passed, no adjustments |
| `CYD_3.2in_ST7789_Resistive.bin` | `cyd32_st7789_res` | Back: "3.2" LCD Display, ESP32-32E" | ST7789P3 IPS 240×320 | XPT2046, shares display SPI | ✅ Passed; needed R/B swap, now the default (inversion on, RGB order) |
| `CYD_3.5in_ST7796_Resistive.bin` | `cyd35_st7796_res` | Back: "3.5" LCD Display, ESP32-32E" | ST7796U 320×480 | XPT2046, shares display SPI | Untested (Tom has no unit); same settings as 4.0" |
| `CYD_4.0in_ST7796_Resistive.bin` | `cyd40_st7796_res` | Back: "4.0" LCD Display, ESP32-32E" | ST7796S 320×480 | XPT2046, shares display SPI | ✅ Passed, no adjustments |
| — | `nm_cyd_c5` | NM-CYD-C5 (ESP32-C5) | 2.8" 320×240 | TBD (stretch goal) | — |

Hardware results are from v0.1.0-alpha.1 on Tom's units (2026-09-30).
Touch calibration worked on every tested board. Boards are named by screen size,
display driver and touch type. The ESP32-32E boards use a different pinout
from the 2.8" ESP32-2432S028 (backlight 27, LED 22/16/17, touch on the
display's SPI bus).

- **Color fixes per unit:** inversion and red/blue order can be toggled on the device and are saved to flash, because panels vary between production runs.
- **Storage:** LittleFS on internal flash (saves, settings, touch calibration, puzzle packs); microSD optional later.
- **UI constraint:** design for resistive taps — large targets, no swipe gestures.

## 2. Development environment
- VS Code + PlatformIO (Windows), C++17, Arduino framework (Arduino-ESP32 3.x via pioarduino).
- Graphics: LovyanGFX (hardware) + LVGL 9 (UI), wired together in `src/hal/lvgl_port.cpp`.

## v1.0 checklist (as of 2026-10-01 code review)

Built and working: everything in sections 3 and 5, stats, hints, themes,
idle-paused clock, web flasher, five board builds.

Open before v1.0 (Tom to decide which are in scope):
- [ ] Section 4 fallback: puzzle packs on LittleFS (not built; on-device
      generation has been reliable, so this may be dropped or replaced by
      technique-based difficulty grading).
- [ ] Difficulty graded only by clue count; no solving-technique grading.
- [ ] Highlighting: in Digit 1st with a digit picked, tapping a placed digit
      doesn't switch the highlight to that digit.
- [ ] Portrait vs landscape decision (Tom).
- [ ] Hardware test of the 2.8" ILI9341 and 3.5" ST7796 builds.
- [ ] SD card on the 2.8" boards (needs a bit-banged touch driver).
- [ ] NM-CYD-C5 board (stretch goal).

## 3. Core mechanics & UX

**Status (v0.1.0-alpha.7):** first pass of everything below is in.
Layout is **portrait** for now (Tom will decide after living with it).
Difficulty is by clue count only (Easy ~38, Medium ~32, Hard ~28,
Expert 24-26); grading by solving technique is planned.

Layout, top to bottom (all screen sizes):
1. Top bar: clock (left), difficulty / "Solved!" / hint prompt (centred),
   3-line hamburger menu (right).
2. Board.
3. Tool row: **Undo**, **Notes**, input-mode button (reads "Digit 1st" or
   "Cell 1st", tap to switch), **Hint**. Widths follow the labels.
4. Digit row 1-9. "Left to place" counts only on 320-wide screens; the
   240-wide boards drop them to give the board more room.

Decisions from Tom's testing:
- (alpha.5) Erase removed: tapping the same digit clears a cell in either mode.
- (alpha.5) Undo kept (this spec requires it).
- (alpha.5) Strong highlight tints with distinct hues; Light/Dark theme.
- (alpha.6) Input mode is a single button showing the current mode, not a
  two-part selector. **Digit 1st is the default.**
- (alpha.6) In Digit 1st, picking a digit clears the cell highlight.
- (alpha.6) Solving: flash the screen twice, then leave the finished board
  on screen. No dialog, no jump to the menu.
- (alpha.6) Hint: two taps (point, then fill). Target order: selected cell
  if empty/wrong, any wrong entry, easiest empty cell. Hinted digits are
  green, locked, undoable, and counted.
- (alpha.7) Clock pauses after 2 minutes without a touch (top bar shows
  "Paused"); it also stops while the menu is open. Off time can't be counted
  (no RTC battery); unattended powered-on time was the stats problem.
- (post alpha.8) Stats screen: "Delete last" and "Clear all", each needing a
  second tap ("Tap again"). New game and Restart need a second tap while a
  game is in progress (moves made or 30 s played).
- (alpha.6) Stats: record Solved and Gave up (leaving a played puzzle for a
  new one) with difficulty, time, hints. CSV on SD where usable, else
  LittleFS. Stats screen: per-difficulty solved / average / best, and the
  most recent games.

- **Dual input modes**
  - *Cell-first:* select a cell, then tap a digit in the 1–9 bank.
  - *Digit-first (brush):* select a digit, then tap cells to place it.
- **Pencil marks:** toggle answer / notes mode. *Auto-clear:* placing a final digit removes it from notes in the same row, column and box.
- **Highlighting:** selected cell's row, column and box get a soft background; tapping a placed digit highlights every matching digit.
- **Number exhaustion:** a digit button dims once that digit is correctly placed 9 times; its counter shows how many are left to place.
- **Undo & persistence:** move stack for undo; board + undo stack saved to LittleFS periodically so the game resumes after power loss.

## 4. Puzzle sourcing
- **Primary:** on-device generation — fill a full grid (backtracking or DLX), then dig holes, checking uniqueness with the solver after each removal.
- **Fallback:** puzzle packs on LittleFS in a compact binary format, built from a public-domain puzzle bank.

## 5. Distribution
- Web flasher (ESP Web Tools on GitHub Pages): https://tomtombombadil.github.io/CYD-Sudoku/ — Chrome/Edge, no software needed.
- GitHub Releases: one merged (factory) `.bin` per board, named as in the table above, flashed at 0x0.

## 6. Licensing
MIT. See `THIRD_PARTY_NOTICES.md` for what may and may not be copied in.
