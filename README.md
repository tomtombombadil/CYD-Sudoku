# CYD Sudoku

A polished, touch-friendly Sudoku for the ESP32 "Cheap Yellow Display" family.

> **Status:** first playable alpha (portrait layout). Puzzles are generated on
> the device; see [docs/SPEC.md](docs/SPEC.md) for what's done and planned.

## How to play

- **Tap a cell, then a digit** to fill it. Tap the same digit again to clear it.
- **Brush:** turn it on, pick a digit, then tap cells to place that digit
  quickly. Erase becomes an eraser brush while Brush is on.
- **Notes:** turn it on to add or remove pencil marks instead of answers.
  Placing a digit removes it from the notes in the same row, column and box.
- **Undo** steps back one action at a time.
- A digit button greys out once all nine of that digit are placed correctly.
- Clashing digits are tinted red.
- **Menu:** new game (Easy, Medium, Hard, Expert), restart, and the display
  and touch settings. The game saves itself and picks up where you left off.

## Install

**Easiest:** open the [web flasher](https://tomtombombadil.github.io/CYD-Sudoku/)
in Chrome or Edge, plug in the board, pick it from the list and click Install.

Or download the `.bin` for your board from [Releases](../../releases) and
flash it at address **0x0** with Espressif's Flash Download Tool.

## Supported boards

Boards are named by screen size, display driver and touch type. Check the
text printed on the back of the board.

| Firmware file | Printed on the back |
|---|---|
| `CYD_2.8in_ILI9341_Resistive.bin` | ESP32-2432S028 (often with R) — usually single micro-USB |
| `CYD_2.8in_ST7789_Resistive.bin` | ESP32-2432S028 (often with R) — usually micro-USB + USB-C |
| `CYD_3.2in_ST7789_Resistive.bin` | 3.2" LCD Display, ESP32-32E, 240x320, Resistive Touch |
| `CYD_3.5in_ST7796_Resistive.bin` | 3.5" LCD Display, ESP32-32E, 320x480, Resistive Touch |
| `CYD_4.0in_ST7796_Resistive.bin` | 4.0" LCD Display, ESP32-32E, 320x480, Resistive Touch |

Confirmed working: 2.8" ST7789, 3.2" ST7789, 4.0" ST7796. The 2.8" ILI9341
and 3.5" ST7796 builds haven't been tested on hardware yet. NM-CYD-C5
(ESP32-C5) support is planned.

Not sure which 2.8" you have? Try ILI9341 first. Wrong colors: see below.
Garbled or blank screen: install the other 2.8" version.

## Building (Windows, VS Code + PlatformIO)

1. Install VS Code and the **PlatformIO IDE** extension.
2. Clone this repo and open the folder in VS Code.
3. In the blue status bar, click the `env:` selector and choose your board
   (e.g. `cyd32_st7789_res` for the 3.2" ST7789 board).
4. Click **Build** (✓), then **Upload** (→) with the board plugged in.
5. **Serial Monitor** (plug icon) shows boot logs at 115200 baud.

## Touch calibration

On first boot (or when the **BOOT** button is held while powering on), the
screen shows corner arrows — tap each tip precisely. The calibration is saved
to flash and reused. To redo it: **Menu → Display & touch → Recalibrate touch**.

## Colors look wrong?

Panels vary between production runs. Open **Menu → Display & touch**: use
**Invert colors** if the background is dark or colors look like a photo
negative, and **Swap red and blue** if the blue digits show as red. The fix
is saved on the board.

## License

[MIT](LICENSE). Third-party components and what may be borrowed from where:
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
