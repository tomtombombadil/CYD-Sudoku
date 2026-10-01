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

static uint32_t fake_ms = 0;
static uint32_t tick() { return fake_ms; }
static void flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

static std::vector<uint16_t> fb;
static int W, H;

static void shot(const std::string& path)
{
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
static void nosave(const game::Game&) {}

int main(int argc, char** argv)
{
    W = atoi(argv[1]); H = atoi(argv[2]);
    const std::string out = argv[3];
    lv_init();
    lv_tick_set_cb(tick);
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
    hooks.firmware_version = "preview";
    hooks.board_name = "Preview";
    ui::game_screen_create(g, hooks);

    // 1. Cell selected that holds a digit -> row/col/box + same digits
    int with_value = -1;
    for (int i = 40; i < 81; ++i) if (g.value(i)) { with_value = i; break; }
    ui::game_screen_tap_cell(with_value);
    shot(out + "_1_select.ppm");

    // 2. A conflicting entry
    int clash = 0;
    for (int j = 0; j < 81; ++j) if (g.given(j) && sudoku::same_unit(first_empty, j)) { clash = g.value(j); break; }
    ui::game_screen_tap_cell(first_empty);
    ui::game_screen_tap_digit(clash);
    shot(out + "_2_conflict.ppm");

    // 3. Brush mode with digit 5 picked, notes on
    ui::game_screen_set_brush(true);
    ui::game_screen_tap_digit(5);
    ui::game_screen_set_notes(true);
    shot(out + "_3_brush_notes.ppm");
    ui::game_screen_set_notes(false);
    ui::game_screen_set_brush(false);

    ui::game_screen_open_menu();
    shot(out + "_4_menu.ppm");
    ui::game_screen_open_settings();
    shot(out + "_5_settings.ppm");
    ui::game_screen_close_overlays();
    fake_ms += 100;
    lv_timer_handler();
    return 0;
}
