// Selects the hardware definition for the board chosen in platformio.ini.
//
// Every board file must provide:
//   class LGFX : public lgfx::LGFX_Device   display + touch + backlight
//   BOARD_NAME                              human-readable name
//   BOARD_TOUCH_RESISTIVE                   1 = XPT2046 (calibrate, no gestures)
//                                           0 = capacitive (GT911, FT6x36, ...)
//   BOARD_PIN_BOOT_BTN                      button held at boot to recalibrate
// Optional: BOARD_PIN_LED_*, BOARD_LED_ACTIVE_LOW, BOARD_PIN_LDR, BOARD_PIN_SD_CS
#pragma once

#if defined(CYD_BOARD_ESP32_2432S028R)
  #include "esp32_2432s028r.hpp"

#elif defined(CYD_BOARD_ESP32_2432S028R_2USB) \
   || defined(CYD_BOARD_ESP32_2432S032)       \
   || defined(CYD_BOARD_ESP32_3248S035)       \
   || defined(CYD_BOARD_CYD40_ST7796)         \
   || defined(CYD_BOARD_NM_CYD_C5)
  #error "This board is listed in platformio.ini but its hardware file in src/boards/ is not written yet."

#else
  #error "No CYD_BOARD_* flag set. Build one of the [env:...] targets in platformio.ini."
#endif
