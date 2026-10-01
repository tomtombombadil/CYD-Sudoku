# CYD Sudoku

A polished, touch-friendly Sudoku for the ESP32 "Cheap Yellow Display" family.

> **Status:** hardware bring-up. The firmware currently boots to a test screen
> (display, color order, touch calibration, LVGL). The game comes next — see
> [docs/SPEC.md](docs/SPEC.md).

## Supported boards

| Board | PlatformIO env | Status |
|---|---|---|
| ESP32-2432S028R 2.8" ILI9341 (single micro-USB) | `cyd28r_ili9341` | ✅ builds, needs hardware test |
| ESP32-2432S028R 2.8" ST7789 (micro-USB + USB-C) | `cyd28r_st7789` | planned |
| ESP32-2432S032 3.2" ST7789 | `cyd32_st7789` | planned |
| ESP32-3248S035 3.5" ST7796 | `cyd35_st7796` | planned |
| 4.0" ST7796 CYD | `cyd40_st7796` | planned |
| NM-CYD-C5 (ESP32-C5) | `nm_cyd_c5` | stretch goal |

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

## License

[MIT](LICENSE). Third-party components and what may be borrowed from where:
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
