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

Decided for v1.0 (Tom, 2026-10-01):
- [x] Difficulty graded by solving technique (section 4), replacing clue
      counts. Puzzle packs dropped: graded generation does the job.
- [x] Digit 1st: tapping a given picks that digit.
- [x] Portrait is final ("easy to hold in one hand and play with the other").
- [x] No "tap again" confirmations anywhere.
- [x] Solve flash = panel invert toggled; brightness slider.
- [x] 2.8" ILI9341 and 3.5" ST7796 ship untested, marked as such on the
      flasher and in the README.
- Out of scope for v1.0: SD on the 2.8" boards, NM-CYD-C5.

Remaining: Tom's hardware test of this build, then he says when to cut
v1.0.0.

## 3. Core mechanics & UX

Layout is **portrait** (final).

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
- (alpha.6) Solving: flash the screen, then leave the finished board on
  screen. No dialog, no jump to the menu. (v1.0) The flash toggles the
  panel's invert bit 6 times, 200 ms apart (not an overlay - too slow).
- (alpha.6) Hint: two taps (point, then fill). Target order: selected cell
  if empty/wrong, any wrong entry, easiest empty cell. Hinted digits are
  green, locked, undoable, and counted.
- (alpha.7) Clock pauses after 2 minutes without a touch (top bar shows
  "Paused"); it also stops while the menu is open. Off time can't be counted
  (no RTC battery); unattended powered-on time was the stats problem.
- (post alpha.8) Stats screen: "Delete last" and "Clear all".
- (v1.0) No confirmation taps anywhere ("this isn't a banking app"): New
  game, Restart, Delete last and Clear all act on the first tap. Hint's
  point-then-fill is a feature, not a confirmation.
- (v1.0) Digit 1st: tapping a given picks its digit. Placing the last of a
  digit greys its button and deselects it at once.
- (v1.0) Display & touch has a brightness slider (saved; floor 20/255).
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

## 4. Puzzle sourcing and difficulty
- On-device generation: fill a random full grid, then remove symmetric pairs
  of clues while the puzzle stays solvable by the techniques its level allows
  (which also guarantees a unique solution). Retry until it actually needs
  its level's technique.
- Grader (`src/game/grader.*`): solves like a person, easiest technique
  first; level = hardest technique used.
  - L1 Easy: naked/hidden singles, 36+ clues.
  - L2 Medium: + pointing/claiming, 30+ clues.
  - L3 Hard: + naked/hidden pairs and triples, X-Wing.
  - L4 Expert: + Swordfish, XY-Wing, XYZ-Wing. Nothing needs chains or
    guessing.
- Calibrated on 8,000 Sudoku Exchange bank puzzles (`tools/grader_check/`):
  level rises with their rating, every logical solve matched the true
  solution.
- Hard/Expert can take a while on the ESP32, so `src/app/puzzle_stock.*`
  keeps 2 per level ready (background task on core 0, lowest priority,
  saved to `/puzzle_stock.bin`). If the stock is empty the game generates
  on the spot with a "Creating…" overlay.
- No puzzle packs: dropped for v1.0.

## 5. Distribution
- Web flasher (ESP Web Tools on GitHub Pages): https://tomtombombadil.github.io/CYD-Sudoku/ — Chrome/Edge, no software needed.
- GitHub Releases: one merged (factory) `.bin` per board, named as in the table above, flashed at 0x0.

## 6. Licensing
MIT. See `THIRD_PARTY_NOTICES.md` for what may and may not be copied in.
