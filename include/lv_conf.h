/**
 * LVGL configuration for CYD Sudoku (LVGL 9.6).
 *
 * Only settings that differ from LVGL's defaults are listed here; everything
 * else falls back to lv_conf_internal.h. The full list of options with
 * descriptions is in .pio/libdeps/<env>/lvgl/lv_conf_template.h.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ---- Color and memory ---------------------------------------------------- */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565  /* 16-bit: what every CYD panel takes */

#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (64 * 1024U)          /* LVGL's private heap for widgets/styles */

/* Only formats we actually render; trims flash. */
#define LV_DRAW_SW_SUPPORT_RGB565          1
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED  1
#define LV_DRAW_SW_SUPPORT_RGB565A8        1
#define LV_DRAW_SW_SUPPORT_RGB888          0
#define LV_DRAW_SW_SUPPORT_XRGB8888        0
#define LV_DRAW_SW_SUPPORT_ARGB8888        1   /* needed for anti-aliased images/fonts blending */
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8              0
#define LV_DRAW_SW_SUPPORT_AL88            0
#define LV_DRAW_SW_SUPPORT_A8              1
#define LV_DRAW_SW_SUPPORT_I1              0

/* ---- Timing -------------------------------------------------------------- */
#define LV_DEF_REFR_PERIOD 16             /* ms between screen refreshes (~60 fps cap) */

/* ---- Logging (Serial, 115200) ------------------------------------------- */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0                   /* routed through lv_log_register_print_cb in lvgl_port.cpp */

/* ---- Fonts --------------------------------------------------------------- */
/* 14: small labels / pencil marks on large screens
 * 20: buttons and menus
 * 28: given/placed digits on 2.8"-3.2" screens
 * Sudoku-specific sizes for 3.5"/4.0" boards will be added with the game UI. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT LV_FONT_DEFAULT_MONTSERRAT_20

/* ---- Features not used --------------------------------------------------- */
#define LV_USE_LOVYAN_GFX 0               /* we use our own glue in lvgl_port.cpp */
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS    0

#endif /* LV_CONF_H */
