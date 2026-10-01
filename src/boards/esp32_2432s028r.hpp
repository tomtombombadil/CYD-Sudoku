// Hardware definition for the 2.8" ESP32-2432S028R ("CYD"), original
// single micro-USB version.
//
// Pin reference: witnessmenow/ESP32-Cheap-Yellow-Display, PINS.md
//   Display  ILI9341 on HSPI : SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, RST = EN
//   Backlight                : GPIO 21 (PWM)
//   Touch    XPT2046 on VSPI : SCLK 25, MOSI 32, MISO 39, CS 33, IRQ 36
//   SD card on VSPI          : SCK 18, MISO 19, MOSI 23, CS 5   (not used yet)
//   RGB LED (active low)     : R 4, G 16, B 17
//   LDR 34, speaker amp 26, BOOT button 0
//
// Do not include this file directly - include boards/board_select.h.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define BOARD_NAME         "ESP32-2432S028R (2.8\" ILI9341)"
#define BOARD_TOUCH_RESISTIVE 1   // XPT2046 - needs calibration, no gestures
#define BOARD_PIN_BOOT_BTN 0
#define BOARD_PIN_LED_R    4
#define BOARD_PIN_LED_G    16
#define BOARD_PIN_LED_B    17
#define BOARD_LED_ACTIVE_LOW 1
#define BOARD_PIN_LDR      34
#define BOARD_PIN_SD_CS    5

// Panel quirks vary between production runs of "the same" board. These can
// be overridden from platformio.ini (-D CYD_PANEL_RGB_ORDER=1 ...) without
// touching this file. The boot test screen shows red/green/blue bars and a
// corner marker so a wrong setting is obvious.
#ifndef CYD_PANEL_RGB_ORDER
#define CYD_PANEL_RGB_ORDER 0   // 1 = swap red and blue
#endif
#ifndef CYD_PANEL_INVERT
#define CYD_PANEL_INVERT 0      // 1 = colors show as a negative image
#endif
#ifndef CYD_SPI_WRITE_HZ
#define CYD_SPI_WRITE_HZ 40000000  // 55 MHz usually works; 40 MHz is the safe default
#endif

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
    lgfx::Touch_XPT2046 _touch;

public:
    LGFX()
    {
        {   // Display SPI bus (HSPI)
            auto cfg = _bus.config();
            cfg.spi_host    = HSPI_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = CYD_SPI_WRITE_HZ;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 14;
            cfg.pin_mosi    = 13;
            cfg.pin_miso    = 12;
            cfg.pin_dc      = 2;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {   // Panel: native portrait 240x320; rotation is set at runtime
            auto cfg = _panel.config();
            cfg.pin_cs           = 15;
            cfg.pin_rst          = -1;   // tied to the ESP32 EN/reset line
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = CYD_PANEL_INVERT;
            cfg.rgb_order        = CYD_PANEL_RGB_ORDER;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false; // HSPI is display-only on this board
            _panel.config(cfg);
        }
        {   // Backlight
            auto cfg = _light.config();
            cfg.pin_bl      = 21;
            cfg.invert      = false;
            cfg.freq        = 12000;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        {   // Touch: XPT2046 on its own pins, driven through VSPI.
            // x/y_min/max are raw 12-bit ADC limits. They are only a
            // starting point: the saved calibration (see touch_cal.cpp)
            // replaces them once the user has calibrated.
            auto cfg = _touch.config();
            cfg.x_min           = 300;
            cfg.x_max           = 3900;
            cfg.y_min           = 200;
            cfg.y_max           = 3750;
            cfg.pin_int         = 36;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 0;
            cfg.spi_host        = VSPI_HOST;
            cfg.freq            = 1000000;
            cfg.pin_sclk        = 25;
            cfg.pin_mosi        = 32;
            cfg.pin_miso        = 39;
            cfg.pin_cs          = 33;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};
