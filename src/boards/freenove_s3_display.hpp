// Freenove ESP32-S3 Display boards (FNK0104). Same file as in
// CYD-Classic-Games (sound left out: CYD Sudoku plays none). So far only the 4.0" touch
// version, FNK0104S: ST7796 320x480 IPS, FT6336U capacitive touch.
//   (Freenove's README calls the 4.0" an ST7789; their own TFT_eSPI setup
//   "FNK0104S_4.0_320x480_ST7796.h", the docs' model table and the
//   datasheet they ship say ST7796 - checked 2026-10-16.)
//
// Pins from Freenove's sketches, TFT_eSPI setup and 4.0" schematic
// (github.com/Freenove/Freenove_ESP32_S3_Display):
//   ESP32-S3R8 (8 MB octal PSRAM) + 16 MB flash, native USB (no UART chip)
//   Display on SPI2 : SCLK 12, MOSI 11, CS 10, DC 46, RST = EN, no MISO
//   Backlight       : GPIO 45 (high = on)
//   Touch FT6336U   : I2C SDA 16, SCL 15, INT 17, RST 18 (address 0x38)
//   Audio ES8311    : same I2C bus (address 0x18); I2S MCLK 4, BCLK 5,
//                     WS 7, DOUT 8 (to the codec), DIN 6 (mic, unused)
//   Speaker amp     : FM8002E/SC8002B, enable GPIO 1 (low = on)
//   microSD         : SDMMC 4-bit, CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47
//   WS2812 RGB LED 42, battery ADC 9 (x2 divider), BOOT button 0
//
// Do not include directly - include boards/board_select.h.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define BOARD_TOUCH_RESISTIVE   0
#define BOARD_PIN_BOOT_BTN      0
#define BOARD_SD_USABLE         1
#define BOARD_SD_MMC            1    // 4-bit SDMMC, not SPI
#define BOARD_PIN_SD_CLK        38
#define BOARD_PIN_SD_CMD        40
#define BOARD_PIN_SD_D0         39
#define BOARD_PIN_SD_D1         41
#define BOARD_PIN_SD_D2         48
#define BOARD_PIN_SD_D3         47
#define BOARD_PIN_AUDIO_EN      1    // low = amplifier on (CYD Sudoku keeps it off)
#define BOARD_I2C_PORT          0    // touch (and the codec) share it
#define BOARD_PIN_I2C_SDA       16
#define BOARD_PIN_I2C_SCL       15
#define BOARD_PIN_BATTERY_ADC   9

template <class PanelT, int kWidth, int kHeight, bool kInvert, bool kRgbOrder>
class LGFX_Freenove_S3 : public lgfx::LGFX_Device
{
    PanelT              _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
    lgfx::Touch_FT5x06  _touch;      // FT6336U speaks the FT5x06 protocol

public:
    LGFX_Freenove_S3()
    {
        {   // The display has SPI2 to itself, on the chip's direct (IO MUX) pins
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 80000000;   // Freenove's own setting for this board
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 12;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 46;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {   // Panel: native portrait
            auto cfg = _panel.config();
            cfg.pin_cs           = 10;
            cfg.pin_rst          = -1;   // tied to EN
            cfg.pin_busy         = -1;
            cfg.panel_width      = kWidth;
            cfg.panel_height     = kHeight;
            cfg.memory_width     = kWidth;
            cfg.memory_height    = kHeight;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.readable         = false;  // no MISO
            cfg.invert           = kInvert;
            cfg.rgb_order        = kRgbOrder;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl      = 45;
            cfg.invert      = false;
            cfg.freq        = 12000;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        {   // Reports in the panel's portrait pixels (Freenove's touch-draw
            // sketch maps it that way), so no calibration is needed.
            auto cfg = _touch.config();
            cfg.x_min           = 0;
            cfg.x_max           = kWidth - 1;
            cfg.y_min           = 0;
            cfg.y_max           = kHeight - 1;
            cfg.pin_int         = 17;
            cfg.pin_rst         = 18;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port        = BOARD_I2C_PORT;
            cfg.i2c_addr        = 0x38;
            cfg.pin_sda         = BOARD_PIN_I2C_SDA;
            cfg.pin_scl         = BOARD_PIN_I2C_SCL;
            cfg.freq            = 400000;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};
