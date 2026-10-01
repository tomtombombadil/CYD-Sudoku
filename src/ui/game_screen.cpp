#include "game_screen.h"

#include <cstdio>
#include <cstring>
#include <lvgl.h>
#include "board_view.h"
#include "theme.h"

namespace ui {

namespace {

constexpr int kEraser = 10;          // brush "digit" that erases

game::Game* G = nullptr;
UiHooks     H{};

// Layout (computed once from the screen size)
int scr_w = 0, scr_h = 0;
bool large = false;                  // 3.5"/4.0" class screens

// Widgets
lv_obj_t* board      = nullptr;
lv_obj_t* status_l   = nullptr;      // difficulty (large screens only)
lv_obj_t* status_r   = nullptr;      // clock (large screens only)
lv_obj_t* digit_btn[10]  = {};
lv_obj_t* digit_cnt[10]  = {};       // "remaining" counters (large screens only)
lv_obj_t* undo_btn   = nullptr;
lv_obj_t* erase_btn  = nullptr;
lv_obj_t* notes_btn  = nullptr;
lv_obj_t* brush_btn  = nullptr;
lv_obj_t* overlay    = nullptr;      // menu / settings / solved

// Input state
int  selected    = -1;
bool notes_mode  = false;
bool brush_mode  = false;
int  brush_digit = 0;                // 0 = none, 1..9, kEraser
bool solved_shown = false;

// Timing
bool     dirty = false;
uint32_t dirty_ms = 0, last_sec_ms = 0, last_save_ms = 0, now_cache = 0;

lv_style_t st_key, st_key_pressed, st_key_checked, st_key_dim;
bool styles_ready = false;

// ---------------------------------------------------------------------------
void init_styles()
{
    if (styles_ready) return;
    styles_ready = true;
    lv_style_init(&st_key);
    lv_style_set_bg_color(&st_key, c_key());
    lv_style_set_bg_opa(&st_key, LV_OPA_COVER);
    lv_style_set_border_color(&st_key, c_key_border());
    lv_style_set_border_width(&st_key, 1);
    lv_style_set_radius(&st_key, 6);
    lv_style_set_text_color(&st_key, c_ink());
    lv_style_set_pad_all(&st_key, 0);

    lv_style_init(&st_key_pressed);
    lv_style_set_bg_color(&st_key_pressed, lv_color_hex(0xE6E9ED));

    lv_style_init(&st_key_checked);
    lv_style_set_bg_color(&st_key_checked, c_key_on());
    lv_style_set_border_color(&st_key_checked, lv_color_hex(0xC99A1F));

    lv_style_init(&st_key_dim);
    lv_style_set_text_color(&st_key_dim, c_key_off_txt());
}

lv_obj_t* make_key(lv_obj_t* parent, int w, int h, lv_event_cb_t cb, intptr_t user)
{
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &st_key, 0);
    lv_obj_add_style(b, &st_key_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(b, &st_key_checked, LV_STATE_CHECKED);
    lv_obj_set_size(b, w, h);
    lv_obj_set_clickable(b, true);
    lv_obj_set_scrollable(b, false);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, reinterpret_cast<void*>(user));
    return b;
}

lv_obj_t* key_label(lv_obj_t* key, const char* text, const lv_font_t* font)
{
    lv_obj_t* l = lv_label_create(key);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_center(l);
    return l;
}

void set_checked(lv_obj_t* o, bool on)
{
    if (!o) return;
    if (on) lv_obj_add_state(o, LV_STATE_CHECKED);
    else    lv_obj_remove_state(o, LV_STATE_CHECKED);
}

void set_dim(lv_obj_t* o, bool dim)
{
    if (!o) return;
    if (dim) lv_obj_add_style(o, &st_key_dim, 0);
    else     lv_obj_remove_style(o, &st_key_dim, 0);
}

void fmt_time(char* out, size_t n, uint32_t s)
{
    if (s >= 3600) snprintf(out, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600),
                            (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    else           snprintf(out, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

void update_status()
{
    if (!status_l) return;
    char t[16];
    fmt_time(t, sizeof t, G->elapsed_s());
    lv_label_set_text(status_l, sudoku::difficulty_name(G->difficulty()));
    lv_label_set_text(status_r, t);
}

void show_solved();

// Push the game state out to every widget.
void update()
{
    BoardHighlight hl;
    hl.selected = selected;
    if (brush_mode && brush_digit >= 1 && brush_digit <= 9) hl.digit = brush_digit;
    else if (selected >= 0) hl.digit = G->value(selected);
    board_set_highlight(board, hl);

    for (int d = 1; d <= 9; ++d) {
        const int placed = G->placed_correct(d);
        set_checked(digit_btn[d], brush_mode && brush_digit == d);
        set_dim(digit_btn[d], placed >= 9);
        if (digit_cnt[d]) {
            if (placed >= 9) lv_label_set_text(digit_cnt[d], "");
            else lv_label_set_text_fmt(digit_cnt[d], "%d", 9 - placed);
        }
    }
    set_checked(notes_btn, notes_mode);
    set_checked(brush_btn, brush_mode);
    set_checked(erase_btn, brush_mode && brush_digit == kEraser);
    set_dim(undo_btn, !G->can_undo());
    update_status();

    if (G->solved() && !solved_shown) {
        solved_shown = true;
        if (H.save) H.save(*G);
        show_solved();
    }
}

void changed()
{
    dirty = true;
    dirty_ms = now_cache;
}

// ---- Input ------------------------------------------------------------------
void on_cell(int i)
{
    if (overlay) return;
    selected = i;
    if (brush_mode && brush_digit) {
        const bool ch = (brush_digit == kEraser) ? G->erase(i) : G->enter(i, brush_digit, notes_mode);
        if (ch) changed();
    }
    update();
}

void on_digit(int d)
{
    if (brush_mode) {
        brush_digit = (brush_digit == d) ? 0 : d;
    } else if (selected >= 0 && G->enter(selected, d, notes_mode)) {
        changed();
    }
    update();
}

void digit_cb(lv_event_t* e) { on_digit(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)))); }

void tool_cb(lv_event_t* e)
{
    switch (reinterpret_cast<intptr_t>(lv_event_get_user_data(e))) {
        case 0:  // Undo
            if (G->undo()) changed();
            break;
        case 1:  // Erase
            if (brush_mode) brush_digit = (brush_digit == kEraser) ? 0 : kEraser;
            else if (selected >= 0 && G->erase(selected)) changed();
            break;
        case 2:  // Notes
            notes_mode = !notes_mode;
            break;
        case 3:  // Brush
            brush_mode = !brush_mode;
            brush_digit = 0;
            break;
        case 4:  // Menu
            game_screen_open_menu();
            return;
    }
    update();
}

// ---- Overlays ---------------------------------------------------------------
int menu_btn_h() { return large ? 52 : 36; }
const lv_font_t* menu_font() { return &lv_font_montserrat_20; }

lv_obj_t* overlay_begin(const char* title)
{
    game_screen_close_overlays();
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, scr_w, scr_h);
    lv_obj_set_style_bg_color(overlay, c_screen(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(overlay, large ? 16 : 10, 0);
    lv_obj_set_style_pad_row(overlay, large ? 10 : 6, 0);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_clickable(overlay, true);      // swallow taps behind it
    lv_obj_set_scrollable(overlay, false);

    lv_obj_t* t = lv_label_create(overlay);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, c_ink(), 0);
    return overlay;
}

lv_obj_t* overlay_text(const char* s, bool muted)
{
    lv_obj_t* l = lv_label_create(overlay);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_text(l, s);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, muted ? c_muted() : c_ink(), 0);
    return l;
}

lv_obj_t* overlay_button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, intptr_t user,
                         bool primary = false)
{
    lv_obj_t* b = make_key(parent, lv_pct(100), menu_btn_h(), cb, user);
    if (primary) lv_obj_add_state(b, LV_STATE_CHECKED);
    key_label(b, text, menu_font());
    return b;
}

void start_new(sudoku::Difficulty d)
{
    // Expert can take a moment on the ESP32; say so before the work starts.
    overlay_begin("New game");
    char msg[48];
    snprintf(msg, sizeof msg, "Creating a %s puzzle...", sudoku::difficulty_name(d));
    overlay_text(msg, true);
    lv_refr_now(nullptr);

    sudoku::Rng rng(H.random_seed ? H.random_seed() : 1);
    G->start(d, rng);
    selected = -1;
    brush_digit = 0;
    solved_shown = false;
    game_screen_close_overlays();
    if (H.save) H.save(*G);
    last_save_ms = now_cache;
    dirty = false;
    update();
}

void new_game_cb(lv_event_t* e)
{
    start_new(static_cast<sudoku::Difficulty>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
}

void menu_cb(lv_event_t* e)
{
    switch (reinterpret_cast<intptr_t>(lv_event_get_user_data(e))) {
        case 0:  // restart puzzle
            G->restart();
            selected = -1;
            solved_shown = false;
            game_screen_close_overlays();
            changed();
            update();
            break;
        case 1: game_screen_open_settings(); break;
        case 2: game_screen_close_overlays(); update(); break;
        case 3: game_screen_open_menu(); break;                // back from settings
        case 4: if (H.toggle_invert) H.toggle_invert(); break;
        case 5: if (H.toggle_swap_rb) H.toggle_swap_rb(); break;
        case 6:
            if (H.save) H.save(*G);
            if (H.recalibrate_touch) H.recalibrate_touch();
            break;
        case 7: game_screen_open_touch_test(); break;
    }
}

void show_solved()
{
    overlay_begin("Solved!");
    char t[16], msg[64];
    fmt_time(t, sizeof t, G->elapsed_s());
    snprintf(msg, sizeof msg, "%s puzzle in %s.", sudoku::difficulty_name(G->difficulty()), t);
    overlay_text(msg, false);
    char again[32];
    snprintf(again, sizeof again, "New %s game", sudoku::difficulty_name(G->difficulty()));
    overlay_button(overlay, again, new_game_cb, static_cast<intptr_t>(G->difficulty()), true);
    overlay_button(overlay, "Menu", menu_cb, 3);
}

void build_layout()
{
    lv_obj_t* scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, c_screen(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);

    scr_w = lv_display_get_horizontal_resolution(nullptr);
    scr_h = lv_display_get_vertical_resolution(nullptr);
    large = scr_w >= 300;

    const int m = 2, gap = 4;
    int key_h  = scr_h / 8;  if (key_h < 36) key_h = 36;  if (key_h > 64) key_h = 64;
    int tool_h = scr_h / 9;  if (tool_h < 32) tool_h = 32; if (tool_h > 52) tool_h = 52;
    const int avail = scr_h - key_h - tool_h - 3 * gap - 2 * m;
    int side = scr_w - 2 * m;
    if (avail < side) side = avail;
    const int cell = (side - 2) / 9;
    const int board_px = 9 * cell + 2;
    const int spare = avail - board_px;               // room left for a status line
    const int status_h = spare >= 20 ? spare : 0;

    int y = m;
    if (status_h) {
        const lv_font_t* f = large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
        status_l = lv_label_create(scr);
        lv_obj_set_style_text_font(status_l, f, 0);
        lv_obj_set_style_text_color(status_l, c_muted(), 0);
        lv_obj_set_pos(status_l, m + 4, y + (status_h - lv_font_get_line_height(f)) / 2);
        status_r = lv_label_create(scr);
        lv_obj_set_style_text_font(status_r, f, 0);
        lv_obj_set_style_text_color(status_r, c_muted(), 0);
        lv_obj_align(status_r, LV_ALIGN_TOP_RIGHT, -(m + 4), y + (status_h - lv_font_get_line_height(f)) / 2);
        y += status_h;
    } else {
        status_l = status_r = nullptr;
    }

    board = board_create(scr, G, cell, on_cell);
    lv_obj_set_pos(board, (scr_w - board_px) / 2, y);
    y += board_px + gap;

    // Tool row
    const int tools = 5;
    const int tool_gap = 4;
    const int tool_w = (scr_w - 2 * m - (tools - 1) * tool_gap) / tools;
    const lv_font_t* tf = large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const char* names[tools] = {"Undo", "Erase", "Notes", "Brush", "Menu"};
    lv_obj_t* tb[tools];
    const int tools_x = (scr_w - (tools * tool_w + (tools - 1) * tool_gap)) / 2;
    for (int k = 0; k < tools; ++k) {
        tb[k] = make_key(scr, tool_w, tool_h, tool_cb, k);
        lv_obj_set_pos(tb[k], tools_x + k * (tool_w + tool_gap), y);
        key_label(tb[k], names[k], tf);
    }
    undo_btn = tb[0]; erase_btn = tb[1]; notes_btn = tb[2]; brush_btn = tb[3];
    y += tool_h + gap;

    // Digit bank
    const int dgap = large ? 4 : 2;
    const int key_w = (scr_w - 2 * m - 8 * dgap) / 9;
    const int keys_x = (scr_w - (9 * key_w + 8 * dgap)) / 2;
    const lv_font_t* kf = key_h >= 56 ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    char s[2] = {0, 0};
    for (int d = 1; d <= 9; ++d) {
        lv_obj_t* k = make_key(scr, key_w, key_h, digit_cb, d);
        lv_obj_set_pos(k, keys_x + (d - 1) * (key_w + dgap), y);
        s[0] = '0' + d;
        lv_obj_t* l = key_label(k, s, kf);
        digit_btn[d] = k;
        digit_cnt[d] = nullptr;
        if (key_h >= 48) {              // room for a "remaining" count
            lv_obj_align(l, LV_ALIGN_CENTER, 0, -7);
            digit_cnt[d] = lv_label_create(k);
            lv_obj_set_style_text_font(digit_cnt[d], &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(digit_cnt[d], c_muted(), 0);
            lv_obj_align(digit_cnt[d], LV_ALIGN_BOTTOM_MID, 0, -3);
        }
    }
}

// ---- Touch test -------------------------------------------------------------
// Plots every raw reading during a tap (first reading red, the rest blue) and
// reports how far the first reading was from where the tap settled. Raw =
// calibrated but not filtered, so this shows what the hardware reports.
constexpr int kMaxDots = 160;
constexpr int kMaxSamples = 64;
lv_obj_t*   tt_dots[kMaxDots] = {};
int         tt_dot_next = 0;
lv_obj_t*   tt_info = nullptr;
lv_timer_t* tt_timer = nullptr;
int16_t     tt_x[kMaxSamples], tt_y[kMaxSamples];
int         tt_n = 0;
int         tt_misses = 0;           // empty readings since the last touch
char        tt_log[4][48];
int         tt_log_n = 0;

void tt_dot(int16_t x, int16_t y, bool first)
{
    lv_obj_t*& d = tt_dots[tt_dot_next];
    tt_dot_next = (tt_dot_next + 1) % kMaxDots;
    if (!d) {
        d = lv_obj_create(overlay);
        lv_obj_remove_style_all(d);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_clickable(d, false);
        lv_obj_set_ignore_layout(d, true);
    }
    const int r = first ? 4 : 2;
    // Positions are relative to the overlay's content area: remove padding
    const int px = lv_obj_get_style_pad_left(overlay, LV_PART_MAIN);
    const int py = lv_obj_get_style_pad_top(overlay, LV_PART_MAIN);
    lv_obj_set_size(d, 2 * r + 1, 2 * r + 1);
    lv_obj_set_pos(d, x - r - px, y - r - py);
    lv_obj_set_style_bg_color(d, first ? c_conflict() : c_entry(), 0);
    if (first) lv_obj_move_foreground(d);
}

void tt_finish_tap()
{
    if (tt_n == 0) return;
    // "Settled" position = median of the second half of the readings
    int16_t xs[kMaxSamples], ys[kMaxSamples];
    const int from = tt_n / 2, m = tt_n - from;
    for (int k = 0; k < m; ++k) { xs[k] = tt_x[from + k]; ys[k] = tt_y[from + k]; }
    for (int a = 0; a < m; ++a) for (int b = a + 1; b < m; ++b) {
        if (xs[b] < xs[a]) { int16_t t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
        if (ys[b] < ys[a]) { int16_t t = ys[a]; ys[a] = ys[b]; ys[b] = t; }
    }
    const int sx = xs[m / 2], sy = ys[m / 2];
    // Shift the log and add this tap
    for (int k = 3; k > 0; --k) memcpy(tt_log[k], tt_log[k - 1], sizeof tt_log[0]);
    snprintf(tt_log[0], sizeof tt_log[0], "First off by %+d,%+d (%d reads)",
             tt_x[0] - sx, tt_y[0] - sy, tt_n);
    if (tt_log_n < 4) ++tt_log_n;
    char text[220];
    int len = snprintf(text, sizeof text, "Red dot = first reading of a tap.");
    for (int k = 0; k < tt_log_n && len < (int)sizeof text; ++k)
        len += snprintf(text + len, sizeof text - len, "\n%s", tt_log[k]);
    lv_label_set_text(tt_info, text);
    tt_n = 0;
}

void tt_timer_cb(lv_timer_t*)
{
    int16_t x, y;
    if (H.raw_touch && H.raw_touch(&x, &y)) {
        tt_misses = 0;
        if (tt_n < kMaxSamples) { tt_x[tt_n] = x; tt_y[tt_n] = y; }
        tt_dot(x, y, tt_n == 0);
        if (tt_n < kMaxSamples) ++tt_n;
    } else if (tt_n && ++tt_misses >= 3) {   // 30 ms without touch = tap over
        tt_finish_tap();
    }
}

void tt_close_cb(lv_event_t*)
{
    game_screen_open_settings();            // closing the overlay stops the test
}

} // namespace

// ---- Public -------------------------------------------------------------------
void game_screen_create(game::Game& g, const UiHooks& hooks)
{
    G = &g;
    H = hooks;
    init_styles();
    selected = -1;
    notes_mode = brush_mode = false;
    brush_digit = 0;
    solved_shown = g.solved();     // a finished saved game doesn't re-celebrate
    overlay = nullptr;
    build_layout();
    update();
}

void game_screen_tick(uint32_t now_ms)
{
    now_cache = now_ms;
    if (!G) return;
    if (last_sec_ms == 0) last_sec_ms = now_ms;
    if (now_ms - last_sec_ms >= 1000) {
        last_sec_ms += 1000;
        if (!overlay && G->active() && !G->solved()) {
            G->add_second();
            update_status();
        }
    }
    // Save 1.5 s after the last move, and every 30 s of play for the clock.
    const bool due = (dirty && now_ms - dirty_ms >= 1500)
                  || (now_ms - last_save_ms >= 30000 && G->active() && !G->solved());
    if (due && H.save) {
        H.save(*G);
        dirty = false;
        last_save_ms = now_ms;
    }
}

void game_screen_tap_cell(int idx)  { on_cell(idx); }
void game_screen_tap_digit(int d)   { on_digit(d); }
void game_screen_set_notes(bool on) { notes_mode = on; update(); }
void game_screen_set_brush(bool on) { brush_mode = on; brush_digit = 0; update(); }

void game_screen_open_menu()
{
    overlay_begin("CYD Sudoku");
    char t[16], line[48];
    fmt_time(t, sizeof t, G->elapsed_s());
    snprintf(line, sizeof line, "Current game: %s, %s", sudoku::difficulty_name(G->difficulty()), t);
    overlay_text(line, true);
    overlay_text("Start a new game:", false);

    lv_obj_t* grid = lv_obj_create(overlay);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(grid, 6, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);
    lv_obj_set_scrollable(grid, false);
    const int half = (scr_w - 2 * (large ? 16 : 10) - 6) / 2;
    for (int d = 0; d < 4; ++d) {
        lv_obj_t* b = make_key(grid, half, menu_btn_h(), new_game_cb, d);
        key_label(b, sudoku::difficulty_name(static_cast<sudoku::Difficulty>(d)), menu_font());
    }
    overlay_button(overlay, "Restart this puzzle", menu_cb, 0);
    overlay_button(overlay, "Display & touch", menu_cb, 1);
    overlay_button(overlay, "Back to game", menu_cb, 2, true);
}

void game_screen_open_settings()
{
    overlay_begin("Display & touch");
    overlay_button(overlay, "Invert colors", menu_cb, 4);
    overlay_button(overlay, "Swap red and blue", menu_cb, 5);
    overlay_button(overlay, "Recalibrate touch", menu_cb, 6);
    if (H.raw_touch) overlay_button(overlay, "Touch test", menu_cb, 7);
    overlay_button(overlay, "Back", menu_cb, 3, true);
    char info[96];
    snprintf(info, sizeof info, "%s\nFirmware %s", H.board_name ? H.board_name : "",
             H.firmware_version ? H.firmware_version : "");
    overlay_text(info, true);
}

void game_screen_open_touch_test()
{
    overlay_begin("Touch test");
    tt_info = overlay_text("Tap anywhere. Red dot = first reading of a tap, blue = the rest.", true);
    tt_n = tt_misses = tt_log_n = tt_dot_next = 0;
    for (auto& d : tt_dots) d = nullptr;
    lv_obj_t* done = make_key(overlay, lv_pct(100), menu_btn_h(), tt_close_cb, 0);
    lv_obj_add_state(done, LV_STATE_CHECKED);
    key_label(done, "Done", menu_font());
    lv_obj_set_ignore_layout(done, true);
    lv_obj_align(done, LV_ALIGN_BOTTOM_MID, 0, 0);
    tt_timer = lv_timer_create(tt_timer_cb, 10, nullptr);
}

void game_screen_close_overlays()
{
    if (tt_timer) {                          // touch test running
        lv_timer_delete(tt_timer);
        tt_timer = nullptr;
        for (auto& d : tt_dots) d = nullptr; // children of the overlay
    }
    if (overlay) {
        // Async: this often runs inside a click handler of a button that
        // lives on the overlay being removed.
        lv_obj_delete_async(overlay);
        overlay = nullptr;
    }
}

} // namespace ui
