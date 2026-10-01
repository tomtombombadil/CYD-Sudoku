// CYD Sudoku - entry point.
//
// Current stage: hardware bring-up. This shows a test screen that checks the
// display (orientation, color order), touch accuracy, and LVGL. The Sudoku UI
// replaces it once the multi-board layer is confirmed on real hardware.
#include <Arduino.h>
#include <lvgl.h>
#include "hal/lvgl_port.h"
#include "hal/panel_prefs.h"
#include "hal/touch_cal.h"

#ifndef CYD_SUDOKU_VERSION
#define CYD_SUDOKU_VERSION "dev"   // CI sets this from the git tag
#endif

#ifndef CYD_ROTATION
#define CYD_ROTATION 0   // 0 = portrait (USB at the bottom on the 2.8" CYD)
#endif

namespace {

lv_obj_t* touch_label = nullptr;
lv_obj_t* touch_dot   = nullptr;

// Moves a dot to wherever the screen is pressed and prints the coordinates,
// so touch accuracy can be judged at the edges and corners.
void screen_pressing_cb(lv_event_t* e)
{
    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_obj_set_pos(touch_dot, p.x - 5, p.y - 5);
    lv_obj_set_hidden(touch_dot, false);
    lv_label_set_text_fmt(touch_label, "Touch: %ld, %ld", (long)p.x, (long)p.y);
    (void)e;
}

void recal_clicked_cb(lv_event_t*)
{
    // Calibration draws with LovyanGFX directly; simplest to reboot afterwards
    // so LVGL starts again with a clean screen.
    LGFX& gfx = lvgl_port_gfx();
    gfx.endWrite();
    touch_cal_run(gfx);
    ESP.restart();
}

// Color fixes for this particular unit; saved to flash and applied at boot.
void invert_clicked_cb(lv_event_t*)
{
    PanelPrefs p = panel_prefs_get();
    p.invert = !p.invert;
    panel_prefs_set(lvgl_port_gfx(), p);
    lv_obj_invalidate(lv_screen_active());
}

void swap_rb_clicked_cb(lv_event_t*)
{
    PanelPrefs p = panel_prefs_get();
    p.swap_rb = !p.swap_rb;
    panel_prefs_set(lvgl_port_gfx(), p);
    lv_obj_invalidate(lv_screen_active());
}

lv_obj_t* text_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_height(btn, 44);             // tall target: resistive-friendly
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return btn;
}

// Label that wraps to the column width (needed on 240-px-wide screens)
lv_obj_t* wrap_label(lv_obj_t* parent, const char* text)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, text);
    return l;
}

lv_obj_t* color_bar(lv_obj_t* parent, lv_color_t c, const char* text)
{
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_style_bg_color(bar, c, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_flex_grow(bar, 1);
    lv_obj_set_height(bar, 28);
    lv_obj_t* l = lv_label_create(bar);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_center(l);
    return bar;
}

void build_test_screen()
{
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x10151c), 0);
    lv_obj_set_scrollable(scr, false);
    lv_obj_set_clickable(scr, true);
    lv_obj_add_event_cb(scr, screen_pressing_cb, LV_EVENT_PRESSING, nullptr);

    // Top-left corner marker: if this isn't in the top-left, the panel is
    // mirrored/rotated wrong for this board.
    lv_obj_t* corner = lv_label_create(scr);
    lv_label_set_text(corner, "TL");
    lv_obj_set_style_text_color(corner, lv_color_hex(0xffd23f), 0);
    lv_obj_align(corner, LV_ALIGN_TOP_LEFT, 4, 2);

    lv_obj_t* col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, lv_pct(92), LV_SIZE_CONTENT);
    lv_obj_align(col, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 5, 0);
    lv_obj_set_clickable(col, false);

    lv_obj_t* title = lv_label_create(col);
    lv_label_set_text(title, "CYD Sudoku");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t* info = wrap_label(col, "");
    lv_label_set_text_fmt(info, "%s\n%ldx%ld  fw " CYD_SUDOKU_VERSION,
                          BOARD_NAME,
                          (long)lv_display_get_horizontal_resolution(nullptr),
                          (long)lv_display_get_vertical_resolution(nullptr));
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info, lv_color_hex(0xb8c4d0), 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);

    // Color-order check: these must read red / green / blue.
    lv_obj_t* bars = lv_obj_create(col);
    lv_obj_remove_style_all(bars);
    lv_obj_set_size(bars, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bars, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bars, 4, 0);
    lv_obj_set_clickable(bars, false);
    color_bar(bars, lv_color_hex(0xd00000), "Red");
    color_bar(bars, lv_color_hex(0x00a000), "Green");
    color_bar(bars, lv_color_hex(0x0000d0), "Blue");

    touch_label = wrap_label(col, "Touch anywhere");
    lv_obj_set_style_text_color(touch_label, lv_color_white(), 0);

    lv_obj_t* hint = wrap_label(col, "Expect a dark background and bars in red, green, blue order");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xb8c4d0), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    // Two side-by-side fix buttons
    lv_obj_t* fixes = lv_obj_create(col);
    lv_obj_remove_style_all(fixes);
    lv_obj_set_size(fixes, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(fixes, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(fixes, 6, 0);
    lv_obj_set_clickable(fixes, false);
    lv_obj_set_flex_grow(text_button(fixes, "Invert", invert_clicked_cb), 1);
    lv_obj_set_flex_grow(text_button(fixes, "Swap R/B", swap_rb_clicked_cb), 1);

    lv_obj_set_width(text_button(col, "Recalibrate touch", recal_clicked_cb), lv_pct(100));

    touch_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(touch_dot);
    lv_obj_set_size(touch_dot, 10, 10);
    lv_obj_set_style_radius(touch_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(touch_dot, lv_color_hex(0xffd23f), 0);
    lv_obj_set_style_bg_opa(touch_dot, LV_OPA_COVER, 0);
    lv_obj_set_hidden(touch_dot, true);
    lv_obj_set_ignore_layout(touch_dot, true);
    lv_obj_set_clickable(touch_dot, false);
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(50);
    Serial.println("\nCYD Sudoku " CYD_SUDOKU_VERSION);

    if (!lvgl_port_init(CYD_ROTATION)) {
        Serial.println("Display init failed - halting");
        while (true) delay(1000);
    }
    build_test_screen();
}

void loop()
{
    uint32_t wait_ms = lvgl_port_loop();
    delay(wait_ms < 5 ? wait_ms : 5);
}
