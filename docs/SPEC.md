# CYD Sudoku — Project Specification

## 1. Target hardware & constraints

**Boards** (one PlatformIO environment each):

| Env | Board | Panel | Touch | Status |
|---|---|---|---|---|
| `cyd28r_ili9341` | ESP32-2432S028R, 2.8", single micro-USB | ILI9341 240×320 | XPT2046 (resistive) | Working (build verified) |
| `cyd28r_st7789` | ESP32-2432S028R, 2.8", micro-USB + USB-C | ST7789 240×320 | XPT2046 | Not started |
| `cyd32_st7789` | ESP32-2432S032, 3.2" | ST7789 240×320 | TBD (R or C variant) | Not started |
| `cyd35_st7796` | ESP32-3248S035, 3.5" | ST7796 320×480 | TBD | Not started |
| `cyd40_st7796` | 4.0" CYD | ST7796 320×480 | TBD | Not started |
| `nm_cyd_c5` | NM-CYD-C5 (ESP32-C5) | 2.8" 320×240 | TBD | Stretch goal |

- **Storage:** LittleFS on internal flash (saves, settings, touch calibration, puzzle packs); microSD optional later.
- **UI constraint:** design for resistive taps — large targets, no swipe gestures.

## 2. Development environment
- VS Code + PlatformIO (Windows), C++17, Arduino framework (Arduino-ESP32 3.x via pioarduino).
- Graphics: LovyanGFX (hardware) + LVGL 9 (UI), wired together in `src/hal/lvgl_port.cpp`.

## 3. Core mechanics & UX
- **Dual input modes**
  - *Cell-first:* select a cell, then tap a digit in the 1–9 bank.
  - *Digit-first (brush):* select a digit, then tap cells to place it.
- **Pencil marks:** toggle answer / notes mode. *Auto-clear:* placing a final digit removes it from notes in the same row, column and box.
- **Highlighting:** selected cell's row, column and box get a soft background; tapping a placed digit highlights every matching digit.
- **Number exhaustion:** a digit button dims once that digit is placed 9 times.
- **Undo & persistence:** move stack for undo; board + undo stack saved to LittleFS periodically so the game resumes after power loss.

## 4. Puzzle sourcing
- **Primary:** on-device generation — fill a full grid (backtracking or DLX), then dig holes, checking uniqueness with the solver after each removal.
- **Fallback:** puzzle packs on LittleFS in a compact binary format, built from a public-domain puzzle bank.

## 5. Distribution
- GitHub Releases: one merged (factory) `.bin` per board.
- Browser flasher page (ESP Web Tools) to flash from Chrome/Edge without installing anything.

## 6. Licensing
MIT. See `THIRD_PARTY_NOTICES.md` for what may and may not be copied in.
