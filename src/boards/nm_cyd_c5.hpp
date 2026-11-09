// NM-CYD-C5 (RockBase): ESP32-C5 version of the 2.8" CYD.
//   ESP32-C5-WROOM-1 (single RISC-V core, 16 MB flash, 8 MB PSRAM),
//   2.8" ST7789 240x320, XPT2046 resistive touch, two USB-C ports
//   (CH340 to UART0, and the chip's own USB).
//
// Pins from RockBase's wiki and their TFT_eSPI setup / PlatformIO pin file
// (github.com/Yverr0y/NM-CYD-C5, Demos/Arduino/libraries/TFT_eSPI/
// User_Setup-NM-CYD-C5.h and Demos/Platformio/nm-cyd-c5/), checked
// 2026-11-08:
//   ONE SPI bus for everything: SCLK 6, MOSI 7, MISO 2
//   Display : CS 23, DC 24, RST = EN, backlight GPIO 25 (high = on)
//   Touch   : XPT2046 on the same bus, CS 1, no IRQ line
//   microSD : same bus, CS 10
//   BOOT key GPIO 28 (schematic: IO28 = BOOT0), speaker input GPIO 26,
//   WS2812 RGB LED 27, extension I2C 8/9, GPS UART 4/5
// RockBase's setup turns inversion off, and their issue #3 adds BGR order.
//
// SD shares the display's bus (the C5 has a single general-purpose SPI),
// so it stays off for now: stats go to LittleFS, as on the 2.8" CYD.
//
// Do not include directly - include boards/board_select.h.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define BOARD_TOUCH_RESISTIVE   1
#define BOARD_PIN_BOOT_BTN      28   // BOOT key (the C5's boot strap pin)
#define BOARD_SD_USABLE         0    // shares the display bus; not wired up yet

template <class PanelT, bool kInvert, bool kRgbOrder>
class LGFX_NM_CYD_C5 : public lgfx::LGFX_Device
{
    PanelT              _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
    lgfx::Touch_XPT2046 _touch;

public:
    LGFX_NM_CYD_C5()
    {
        {   // The C5's general-purpose SPI, shared by display, touch and SD
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = CYD_SPI_WRITE_HZ;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 6;
            cfg.pin_mosi    = 7;
            cfg.pin_miso    = 2;
            cfg.pin_dc      = 24;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {   // Panel: native portrait
            auto cfg = _panel.config();
            cfg.pin_cs           = 23;
            cfg.pin_rst          = -1;   // tied to EN
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.readable         = true;
            cfg.invert           = kInvert;
            cfg.rgb_order        = kRgbOrder;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;   // touch is on this bus too
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl      = 25;
            cfg.invert      = false;
            cfg.freq        = 12000;
            cfg.pwm_channel = 0;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        {   // Touch on the display's bus, like the ESP32-32E boards. Same
            // starting calibration range as the 2.8" CYD; each unit is
            // calibrated on first boot anyway.
            auto cfg = _touch.config();
            cfg.x_min           = 200;
            cfg.x_max           = 3700;
            cfg.y_min           = 240;
            cfg.y_max           = 3800;
            cfg.pin_int         = -1;      // no IRQ line on this board
            cfg.bus_shared      = true;
            cfg.offset_rotation = 0;
            cfg.spi_host        = SPI2_HOST;
            cfg.freq            = 1000000; // as on the ESP32-32E boards (settling time)
            cfg.pin_sclk        = 6;
            cfg.pin_mosi        = 7;
            cfg.pin_miso        = 2;
            cfg.pin_cs          = 1;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};
