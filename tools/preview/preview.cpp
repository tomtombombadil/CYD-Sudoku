// PC preview: runs the real game UI (LVGL + src/ui + src/game) into an
// in-memory framebuffer and writes screenshots as .ppm files.
// Build: see tools/preview/build.sh (used by Claude / CI, not needed on Windows).
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "game/game.h"
#include "ui/game_screen.h"
#include "game/stats.h"

static uint32_t fake_ms = 0;
static uint32_t tick() { return fake_ms; }
static void flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static std::vector<uint16_t> fb;
static int W, H;

static void shot(const std::string& path)
{
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    fprintf(stderr, "%s: LVGL heap used %u%%, free %u, biggest free %u\n", path.c_str(),
            (unsigned)mon.used_pct, (unsigned)mon.free_size, (unsigned)mon.free_biggest_size);
    fake_ms += 50;
    lv_timer_handler();
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(nullptr);
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; ++i) {
        const uint16_t p = fb[i];
        const uint8_t rgb[3] = {uint8_t((p >> 11) << 3), uint8_t(((p >> 5) & 63) << 2), uint8_t((p & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("wrote %s\n", path.c_str());
}

static uint32_t seed() { return 4242; }

static bool fake_stats(stats::Summary& s)
{
    s = stats::Summary{};
    const uint32_t data[][4] = {   // difficulty, result, seconds, hints
        {0,0,301,0},{0,0,275,0},{1,0,512,0},{1,1,840,1},{1,0,468,2},{2,0,1104,0},
        {0,0,250,0},{2,1,1500,0},{1,0,455,0},{3,0,1922,3},{1,0,430,0}};
    for (auto& d : data) {
        stats::Record r; r.difficulty = d[0]; r.result = d[1] ? stats::Result::GaveUp : stats::Result::Solved;
        r.seconds = d[2]; r.hints = d[3]; s.add(r);
    }
    return true;
}
static const char* fake_location() { return "SD card"; }

// Simulated stylus taps for the touch-test screenshot: the first reading of
// each tap lands one cell low, the rest on target (Tom's 4.0" symptom).
static int fake_step = -1;
static bool fake_touch(int16_t* x, int16_t* y)
{
    static const int16_t taps[3][2] = {{100, 200}, {200, 300}, {60, 380}};
    if (fake_step < 0) return false;
    const int tap = fake_step / 14, k = fake_step % 14;
    if (tap >= 3 || k >= 9) return false;     // 9 readings, then 5 empty
    *x = taps[tap][0] + (k ? (k % 3) - 1 : 0);
    *y = taps[tap][1] + (k ? (k % 2) : 35);
    return true;
}
static void nosave(const game::Game&) {}

int main(int argc, char** argv)
{
    W = atoi(argv[1]); H = atoi(argv[2]);
    const std::string out = argv[3];
    lv_init();
    lv_tick_set_cb(tick);
    lv_log_register_print_cb([](lv_log_level_t, const char* m) { fputs(m, stderr); });
    fb.assign(W * H, 0);
    lv_display_t* d = lv_display_create(W, H);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(d, flush);
    lv_display_set_buffers(d, fb.data(), nullptr, W * H * 2, LV_DISPLAY_RENDER_MODE_DIRECT);

    static game::Game g;
    sudoku::Rng rng(20261001);
    g.start(sudoku::Difficulty::Medium, rng);

    // Solve on the side so the preview can enter some right and wrong digits
    sudoku::Grid p{}, sol{};
    for (int i = 0; i < 81; ++i) p.c[i] = g.given(i) ? g.value(i) : 0;
    sudoku::count_solutions(p, 2, &sol);
    int filled = 0, first_empty = -1, second_empty = -1;
    for (int i = 0; i < 81 && filled < 14; ++i) {
        if (g.given(i)) continue;
        if (first_empty < 0) { first_empty = i; continue; }
        if (second_empty < 0) { second_empty = i; continue; }
        if (i % 3 == 0) { g.enter(i, sol.c[i], false); ++filled; }
    }
    // Notes in a couple of empty cells
    for (int i = 0, n = 0; i < 81 && n < 6; ++i) {
        if (g.given(i) || g.value(i)) continue;
        if (i == first_empty || i == second_empty) continue;
        for (int dd = 1; dd <= 9; ++dd) if ((dd + i) % 3 == 0) g.enter(i, dd, true);
        ++n;
    }
    for (int k = 0; k < 125; ++k) g.add_second();

    ui::UiHooks hooks{};
    hooks.random_seed = seed;
    hooks.save = nosave;
    hooks.raw_touch = fake_touch;
    hooks.load_stats = fake_stats;
    hooks.stats_location = fake_location;
    hooks.delete_last_stat = [] { return true; };
    hooks.clear_stats = [] { return true; };
    hooks.firmware_version = "preview";
    hooks.board_name = "Preview";
    ui::UiSettings settings;
    ui::game_screen_create(g, hooks, settings);

    int with_value = -1;
    for (int i = 40; i < 81; ++i) if (g.value(i) && !g.given(i)) { with_value = i; break; }
    int clash = 0;
    for (int j = 0; j < 81; ++j) if (g.given(j) && sudoku::same_unit(first_empty, j)) { clash = g.value(j); break; }

    for (int t = 0; t < 2; ++t) {
        const std::string pre = out + (t ? "_dark" : "_light");
        ui::game_screen_set_theme(t ? ui::Theme::Dark : ui::Theme::Light);
        ui::game_screen_set_input_mode(ui::InputMode::CellFirst);

        // 1. Cell-first, a cell with a digit selected: row/col/box + same digits
        ui::game_screen_tap_cell(with_value);
        shot(pre + "_1_select.ppm");

        // 2. A clashing entry
        ui::game_screen_tap_cell(first_empty);
        ui::game_screen_tap_digit(clash);
        shot(pre + "_2_conflict.ppm");
        ui::game_screen_tap_digit(clash);           // same digit again clears it

        // 3. Digit first with 5 picked, notes on
        ui::game_screen_set_input_mode(ui::InputMode::DigitFirst);
        ui::game_screen_tap_digit(5);
        ui::game_screen_set_notes(true);
        shot(pre + "_3_digit_first_notes.ppm");
        ui::game_screen_set_notes(false);
        ui::game_screen_tap_digit(5);

        // 4. Hint pointing at a cell (first tap)
        ui::game_screen_hint();
        shot(pre + "_4_hint.ppm");
        ui::game_screen_hint();                     // second tap fills it

        ui::game_screen_open_menu();
        shot(pre + "_5_menu.ppm");
        if (t == 0) {                                // first tap on Easy: asks to confirm
            ui::game_screen_menu_tap_new_game(0);
            shot(pre + "_5b_menu_confirm.ppm");
        }
        ui::game_screen_open_stats();
        shot(pre + "_6_stats.ppm");
        ui::game_screen_open_settings();
        shot(pre + "_7_settings.ppm");
        ui::game_screen_close_overlays();
        fake_ms += 50; lv_timer_handler();
    }

    // Solve with hints: mid-flash and after
    ui::game_screen_set_theme(ui::Theme::Light);
    for (int k = 0; k < 200 && !g.solved(); ++k) ui::game_screen_hint();
    shot(out + "_light_8_solved_flash.ppm");
    for (int k = 0; k < 10; ++k) { fake_ms += 100; lv_timer_handler(); }
    shot(out + "_light_9_solved.ppm");

    ui::game_screen_set_theme(ui::Theme::Dark);
    ui::game_screen_open_touch_test();
    for (fake_step = 0; fake_step < 3 * 14; ++fake_step) { fake_ms += 10; lv_timer_handler(); }
    fake_step = -1;
    for (int k = 0; k < 5; ++k) { fake_ms += 10; lv_timer_handler(); }
    shot(out + "_dark_6_touch_test.ppm");
    ui::game_screen_close_overlays();
    fake_ms += 100;
    lv_timer_handler();
    return 0;
}
