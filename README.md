# CYD Sudoku

A polished, touch-friendly Sudoku for the ESP32 "Cheap Yellow Display" family.

> **Status:** hardware bring-up. The firmware currently boots to a test screen
> (display, color order, touch calibration, LVGL). The game comes next — see
> [docs/SPEC.md](docs/SPEC.md).

## Supported boards

| Board (what's printed on it) | PlatformIO env | Status |
|---|---|---|
| ESP32-2432S028 2.8", ILI9341 panel | `cyd28_ili9341` | builds, needs hardware test |
| ESP32-2432S028 2.8", ST7789 panel | `cyd28_st7789` | builds, needs hardware test |
| "3.2" LCD Display, ESP32-32E, 240x320" (LCDwiki E32R32P) | `lcdwiki32_st7789` | builds, needs hardware test |
| "3.5" LCD Display, ESP32-32E, 320x480" (LCDwiki E32R35T) | `lcdwiki35_st7796` | builds, needs hardware test |
| "4.0" LCD Display, ESP32-32E, 320x480" (LCDwiki E32R40T) | `lcdwiki40_st7796` | builds, needs hardware test |
| NM-CYD-C5 (ESP32-C5) | `nm_cyd_c5` | stretch goal |

Not sure whether a 2.8" board is ILI9341 or ST7789? Flash either one: if
colors look wrong, use the **Invert** / **Swap R/B** buttons first; if the
picture is garbled or blank, try the other build.

## Building (Windows, VS Code + PlatformIO)

1. Install VS Code and the **PlatformIO IDE** extension.
2. Clone this repo and open the folder in VS Code.
3. In the blue status bar, click the `env:` selector and choose your board.
4. Click **Build** (✓), then **Upload** (→) with the board plugged in.
5. **Serial Monitor** (plug icon) shows boot logs at 115200 baud.

## Flashing a release without building

Each [release](../../releases) has one `.bin` per board. It is a complete
(merged) image — flash it at address **0x0** with Espressif's Flash Download
Tool or esptool. A browser-based flasher is planned.

## Touch calibration

On first boot (or when the **BOOT** button is held while powering on), the
screen shows corner arrows — tap each tip precisely. The calibration is saved
to flash and reused. The test screen also has a **Recalibrate touch** button.

## Colors look wrong?

Panels vary between production runs. On the test screen, tap **Invert** if
the background is light or colors look like a photo negative, and
**Swap R/B** if the red and blue bars are swapped. The fix is saved on the
board.

## License

[MIT](LICENSE). Third-party components and what may be borrowed from where:
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
