// CYD Sudoku - entry point.
#include <Arduino.h>
#include <esp_random.h>
#include <lvgl.h>
#include "app/save_store.h"
#include "app/settings_store.h"
#include "app/stats_store.h"
#include "game/game.h"
#include "hal/lvgl_port.h"
#include "hal/panel_prefs.h"
#include "hal/touch_cal.h"
#include "ui/game_screen.h"

#ifndef CYD_SUDOKU_VERSION
#define CYD_SUDOKU_VERSION "dev"   // CI sets this from the git tag / commit
#endif

#ifndef CYD_ROTATION
#define CYD_ROTATION 0             // 0 = portrait
#endif

namespace {

game::Game the_game;               // ~4 KB: static, not on the task stack

// esp_random() is weaker with the radio off; mixing in the microsecond
// timer (when the player tapped) makes repeated puzzles even less likely.
uint32_t hw_seed() { return esp_random() ^ (micros() * 2654435761u); }

// Put the on-board extras in a known, quiet state: RGB LED off and the
// audio amplifier disabled (floating pins can light the LED or make a
// connected speaker hiss).
void quiet_peripherals()
{
#ifdef BOARD_PIN_LED_R
    const uint8_t off = BOARD_LED_ACTIVE_LOW ? HIGH : LOW;
    const uint8_t leds[3] = {BOARD_PIN_LED_R, BOARD_PIN_LED_G, BOARD_PIN_LED_B};
    for (uint8_t p : leds) { pinMode(p, OUTPUT); digitalWrite(p, off); }
#endif
#ifdef BOARD_PIN_AUDIO_EN
    pinMode(BOARD_PIN_AUDIO_EN, OUTPUT);
    digitalWrite(BOARD_PIN_AUDIO_EN, HIGH);   // high = amplifier off
#endif
}

// Panel color fixes change how the hardware shows every pixel; the image in
// LVGL is unchanged, so a full redraw pushes it out again.
void redraw_all()
{
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
}

void toggle_invert()
{
    PanelPrefs p = panel_prefs_get();
    p.invert = !p.invert;
    panel_prefs_set(lvgl_port_gfx(), p);
    redraw_all();
}

void toggle_swap_rb()
{
    PanelPrefs p = panel_prefs_get();
    p.swap_rb = !p.swap_rb;
    panel_prefs_set(lvgl_port_gfx(), p);
    redraw_all();
}

void recalibrate()
{
    // Calibration draws with LovyanGFX directly; restarting afterwards gives
    // LVGL a clean screen. The game was saved by the caller.
    LGFX& gfx = lvgl_port_gfx();
    gfx.waitDMA();
    gfx.endWrite();
    touch_cal_run(gfx);
    ESP.restart();
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(50);
    Serial.println("\nCYD Sudoku " CYD_SUDOKU_VERSION);
    quiet_peripherals();

    if (!lvgl_port_init(CYD_ROTATION)) {
        Serial.println("Display init failed - halting");
        while (true) delay(1000);
    }

    if (!save_store_load(the_game) || !the_game.active()) {
        sudoku::Rng rng(hw_seed());
        the_game.start(sudoku::Difficulty::Easy, rng);
        save_store_save(the_game);
    }

    ui::UiHooks hooks{};
    hooks.random_seed       = hw_seed;
    hooks.save              = save_store_save;
    hooks.save_settings     = settings_store_save;
    hooks.toggle_invert     = toggle_invert;
    hooks.toggle_swap_rb    = toggle_swap_rb;
    hooks.recalibrate_touch = recalibrate;
    hooks.raw_touch         = lvgl_port_raw_touch;
    hooks.record_stat       = stats_store_record;
    hooks.load_stats        = stats_store_load;
    hooks.stats_location    = stats_store_location;
    hooks.delete_last_stat  = stats_store_delete_last;
    hooks.clear_stats       = stats_store_clear;
    hooks.firmware_version  = CYD_SUDOKU_VERSION;
    hooks.board_name        = BOARD_NAME;
    ui::game_screen_create(the_game, hooks, settings_store_load());
}

void loop()
{
    const uint32_t wait_ms = lvgl_port_loop();
    ui::game_screen_tick(millis());
    delay(wait_ms < 5 ? wait_ms : 5);
}
