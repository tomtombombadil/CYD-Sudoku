// The Sudoku game screen: top bar, board, tool row, digit bank, menus.
// Hardware-specific actions come in through UiHooks so this file also builds
// in the PC preview tool (tools/preview).
#pragma once

#include <cstdint>
#include "game/game.h"
#include "theme.h"

namespace ui {

struct UiHooks {
    uint32_t (*random_seed)();                 // fresh seed for a new puzzle
    void (*save)(const game::Game&);           // persist the game now
    void (*save_settings)(const UiSettings&);  // persist theme / input mode
    void (*toggle_invert)();                   // panel color fixes
    void (*toggle_swap_rb)();
    void (*recalibrate_touch)();               // may not return (device restarts)
    // Unfiltered touch reading for the touch test (nullptr = not available)
    bool (*raw_touch)(int16_t* x, int16_t* y);
    const char* firmware_version;
    const char* board_name;
};

// Builds the screen for a game that is already loaded or started.
void game_screen_create(game::Game& g, const UiHooks& hooks, const UiSettings& settings);

// Call often from loop(): runs the clock and debounced autosave.
void game_screen_tick(uint32_t now_ms);

// Change theme and redraw everything (also used by the settings menu).
void game_screen_set_theme(Theme t);

// ---- Used by the PC preview to stage screenshots ---------------------------
void game_screen_tap_cell(int idx);
void game_screen_tap_digit(int d);
void game_screen_set_notes(bool on);
void game_screen_set_input_mode(InputMode m);
void game_screen_open_menu();
void game_screen_open_settings();
void game_screen_open_touch_test();
void game_screen_close_overlays();

} // namespace ui
