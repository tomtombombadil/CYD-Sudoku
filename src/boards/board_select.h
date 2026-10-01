// Selects the hardware definition for the board chosen in platformio.ini.
// This is the ONLY file that tests CYD_BOARD_* flags.
//
// Every board must end up defining:
//   class LGFX : public lgfx::LGFX_Device   display + touch + backlight
//   BOARD_NAME                              human-readable name
//   BOARD_TOUCH_RESISTIVE                   1 = XPT2046 (calibrate, no gestures)
//   BOARD_PIN_BOOT_BTN                      held at power-up to recalibrate touch
// Optional: BOARD_PIN_LED_*, BOARD_LED_ACTIVE_LOW, BOARD_PIN_LDR,
//           BOARD_PIN_SD_CS, BOARD_PIN_AUDIO_EN, BOARD_PIN_BATTERY_ADC
//
// Colors: each board's default inversion is a best guess. If a particular
// unit shows a negative image or swapped red/blue, fix it on the device
// (test screen buttons, saved to flash) - no rebuild needed.
#pragma once

// Display SPI clock. 40 MHz is safe on every board; many run at 55-80 MHz.
#ifndef CYD_SPI_WRITE_HZ
#define CYD_SPI_WRITE_HZ 40000000
#endif

#if defined(CYD_BOARD_SUNTON_2432S028_ILI9341)
  #include "sunton_cyd28.hpp"
  #define BOARD_NAME "ESP32-2432S028 2.8\" ILI9341"
  class LGFX : public LGFX_SuntonCYD28<lgfx::Panel_ILI9341, false> {};

#elif defined(CYD_BOARD_SUNTON_2432S028_ST7789)
  #include "sunton_cyd28.hpp"
  #define BOARD_NAME "ESP32-2432S028 2.8\" ST7789"
  class LGFX : public LGFX_SuntonCYD28<lgfx::Panel_ST7789, false> {};

#elif defined(CYD_BOARD_LCDWIKI_E32R32P)
  #include "lcdwiki_esp32e.hpp"
  #define BOARD_NAME "LCDwiki E32R32P 3.2\" ST7789"   // IPS panel: needs inversion on
  class LGFX : public LGFX_LcdwikiEsp32E<lgfx::Panel_ST7789P3, 240, 320, true> {};

#elif defined(CYD_BOARD_LCDWIKI_E32R35T)
  #include "lcdwiki_esp32e.hpp"
  #define BOARD_NAME "LCDwiki E32R35T 3.5\" ST7796"
  class LGFX : public LGFX_LcdwikiEsp32E<lgfx::Panel_ST7796, 320, 480, false> {};

#elif defined(CYD_BOARD_LCDWIKI_E32R40T)
  #include "lcdwiki_esp32e.hpp"
  #define BOARD_NAME "LCDwiki E32R40T 4.0\" ST7796"
  class LGFX : public LGFX_LcdwikiEsp32E<lgfx::Panel_ST7796, 320, 480, false> {};

#elif defined(CYD_BOARD_NM_CYD_C5)
  #error "NM-CYD-C5 is listed in platformio.ini but its board file is not written yet."

#else
  #error "No CYD_BOARD_* flag set. Build one of the [env:...] targets in platformio.ini."
#endif
