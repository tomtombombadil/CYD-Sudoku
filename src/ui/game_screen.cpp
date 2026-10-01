#include "game_screen.h"

#include <cstdio>
#include <cstring>
#include <lvgl.h>
#include "board_view.h"
#include "theme.h"

namespace ui {

namespace {

game::Game* G = nullptr;
UiHooks     H{};
UiSettings  S{};

// Layout (computed from the screen size in build_layout)
int  scr_w = 0, scr_h = 0;
bool large = false;                  // 320-px-wide screens (3.5"/4.0")

// Widgets
lv_obj_t* board      = nullptr;
lv_obj_t* clock_l    = nullptr;
lv_obj_t* center_l   = nullptr;      // difficulty, "Solved!" or a hint prompt
lv_obj_t* digit_btn[10] = {};
lv_obj_t* digit_cnt[10] = {};        // "left to place" (large screens only)
lv_obj_t* undo_btn   = nullptr;
lv_obj_t* notes_btn  = nullptr;
lv_obj_t* mode_btn   = nullptr;
lv_obj_t* mode_lbl   = nullptr;
lv_obj_t* hint_btn   = nullptr;
lv_obj_t* overlay    = nullptr;      // menu / settings / stats / touch test

// Input state
int  selected     = -1;
bool notes_mode   = false;
int  brush_digit  = 0;               // Digit 1st mode: the digit being placed
int  hint_cell    = -1;              // cell the last Hint tap pointed at
bool solved_seen  = false;           // solve already celebrated + recorded
bool idle_paused  = false;           // clock stopped: no touches for a while

// The clock stops after this long with no touch, so time the board sits on
// unattended doesn't count. The next touch starts it again.
constexpr uint32_t kIdlePauseMs = 2 * 60 * 1000;

// Timing
bool     dirty = false;
uint32_t dirty_ms = 0, last_sec_ms = 0, last_save_ms = 0, now_cache = 0;

lv_style_t st_key, st_key_pressed, st_key_checked, st_key_dim;
bool styles_inited = false;

bool digit_first() { return S.input == InputMode::DigitFirst; }
bool finished()    { return G->solved(); }

// ---------------------------------------------------------------------------
// Styles are set from the current palette; calling again after a theme change
// updates them in place.
void apply_styles()
{
    const Palette& p = pal();
    if (!styles_inited) {
        styles_inited = true;
        lv_style_init(&st_key);
        lv_style_init(&st_key_pressed);
        lv_style_init(&st_key_checked);
        lv_style_init(&st_key_dim);
    }
    lv_style_set_bg_color(&st_key, p.key);
    lv_style_set_bg_opa(&st_key, LV_OPA_COVER);
    lv_style_set_border_color(&st_key, p.key_border);
    lv_style_set_border_width(&st_key, 1);
    lv_style_set_radius(&st_key, 6);
    lv_style_set_text_color(&st_key, p.ink);
    lv_style_set_pad_all(&st_key, 0);

    lv_style_set_bg_color(&st_key_pressed, p.key_pressed);
    lv_style_set_bg_opa(&st_key_pressed, LV_OPA_COVER);

    lv_style_set_bg_color(&st_key_checked, p.key_on);
    lv_style_set_bg_opa(&st_key_checked, LV_OPA_COVER);
    lv_style_set_border_color(&st_key_checked, p.key_on);
    lv_style_set_text_color(&st_key_checked, p.key_on_text);

    lv_style_set_text_color(&st_key_dim, p.key_dim_text);

    lv_obj_report_style_change(nullptr);
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

int text_width(const char* s, const lv_font_t* f)
{
    lv_point_t sz;
    lv_text_get_size(&sz, s, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

void update_status()
{
    if (!clock_l) return;
    char t[16];
    stats::format_time(t, sizeof t, G->elapsed_s());
    lv_label_set_text(clock_l, t);
    if (finished())          lv_label_set_text(center_l, "Solved!");
    else if (idle_paused)    lv_label_set_text(center_l, "Paused");
    else if (hint_cell >= 0) lv_label_set_text(center_l, "Tap Hint to fill");
    else                     lv_label_set_text(center_l, sudoku::difficulty_name(G->difficulty()));
}

void save_settings()
{
    if (H.save_settings) H.save_settings(S);
}

void celebrate();

// Push the game state out to every widget.
void update()
{
    const bool done = finished();
    BoardHighlight hl;
    hl.selected = done ? -1 : selected;
    if (!done) {
        if (digit_first() && brush_digit) hl.digit = brush_digit;
        else if (selected >= 0) hl.digit = G->value(selected);
    }
    board_set_highlight(board, hl);

    for (int d = 1; d <= 9; ++d) {
        const bool digit_done = G->placed_correct(d) >= 9;
        set_checked(digit_btn[d], !done && digit_first() && brush_digit == d);
        set_dim(digit_btn[d], digit_done);
        if (digit_cnt[d]) {
            // Left to place = 9 minus how many are on the board now, right or
            // wrong (what the player can count). Blank once the digit is done.
            const int left = 9 - G->count(d);
            if (digit_done) lv_label_set_text(digit_cnt[d], "");
            else            lv_label_set_text_fmt(digit_cnt[d], "%d", left > 0 ? left : 0);
        }
    }
    set_checked(notes_btn, notes_mode && !done);
    lv_label_set_text(mode_lbl, digit_first() ? "Digit 1st" : "Cell 1st");
    set_dim(undo_btn, done || !G->can_undo());
    set_dim(notes_btn, done);
    set_dim(mode_btn, done);
    set_dim(hint_btn, done);
    set_checked(hint_btn, hint_cell >= 0 && !done);
    update_status();

    if (done && !solved_seen) {
        solved_seen = true;
        if (H.save) H.save(*G);
        if (H.record_stat) {
            stats::Record r;
            r.difficulty = static_cast<uint8_t>(G->difficulty());
            r.result = stats::Result::Solved;
            r.seconds = G->elapsed_s();
            r.hints = static_cast<uint8_t>(G->hints_used());
            H.record_stat(r);
        }
        celebrate();
    }
}

void changed()
{
    dirty = true;
    dirty_ms = now_cache;
}

// ---- Celebration --------------------------------------------------------------
// Two quick amber flashes over the finished board, then the solved board stays
// on screen. Nothing else opens; the menu is there when the player wants it.
lv_obj_t*   flash_obj = nullptr;
lv_timer_t* flash_timer = nullptr;
int         flash_step = 0;

void flash_cb(lv_timer_t*)
{
    ++flash_step;
    if (flash_step >= 4) {
        lv_timer_delete(flash_timer);
        flash_timer = nullptr;
        lv_obj_delete(flash_obj);
        flash_obj = nullptr;
        return;
    }
    lv_obj_set_style_bg_opa(flash_obj, (flash_step % 2) ? LV_OPA_TRANSP : LV_OPA_60, 0);
}

void celebrate()
{
    if (flash_obj) return;
    flash_obj = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(flash_obj);
    lv_obj_set_size(flash_obj, scr_w, scr_h);
    lv_obj_set_style_bg_color(flash_obj, pal().key_on, 0);
    lv_obj_set_style_bg_opa(flash_obj, LV_OPA_60, 0);
    lv_obj_set_clickable(flash_obj, false);
    flash_step = 0;
    flash_timer = lv_timer_create(flash_cb, 180, nullptr);
}

// ---- Input ------------------------------------------------------------------
// Cell 1st:  tap a cell, then a digit. The same digit again clears it.
// Digit 1st: tap a digit, then cells. Tapping a cell that already holds
//            that digit clears it; another digit is replaced. Picking a
//            digit clears the cell highlight.
// Notes mode applies to both: digits toggle pencil marks instead.
void on_cell(int i)
{
    if (overlay || finished()) return;
    selected = i;
    hint_cell = -1;
    if (digit_first() && brush_digit && G->enter(i, brush_digit, notes_mode)) changed();
    update();
}

void on_digit(int d)
{
    if (finished()) return;
    hint_cell = -1;
    if (digit_first()) {
        brush_digit = (brush_digit == d) ? 0 : d;
        selected = -1;
    } else if (selected >= 0 && G->enter(selected, d, notes_mode)) {
        changed();
    }
    update();
}

// First tap: point at the cell (selected + highlighted, top bar prompts).
// Second tap on the same cell: fill it with the correct digit.
void on_hint()
{
    if (finished()) return;
    if (hint_cell >= 0 && hint_cell == selected && G->hint_target(selected) == hint_cell) {
        if (G->apply_hint(hint_cell)) changed();
        hint_cell = -1;
    } else {
        const int t = G->hint_target(selected);
        if (t < 0) return;
        selected = t;
        hint_cell = t;
        brush_digit = 0;
    }
    update();
}

void toggle_input_mode()
{
    S.input = digit_first() ? InputMode::CellFirst : InputMode::DigitFirst;
    brush_digit = 0;
    save_settings();
}

void digit_cb(lv_event_t* e) { on_digit(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)))); }

void tool_cb(lv_event_t* e)
{
    const intptr_t id = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    if (id == 4) { game_screen_open_menu(); return; }
    if (finished()) return;
    if (id != 3) hint_cell = -1;
    switch (id) {
        case 0: if (G->undo()) changed(); break;
        case 1: notes_mode = !notes_mode; break;
        case 2: toggle_input_mode(); break;
        case 3: on_hint(); return;
    }
    update();
}

// ---- Overlays ---------------------------------------------------------------
int menu_btn_h() { return large ? 50 : 32; }
const lv_font_t* menu_font() { return large ? &lv_font_montserrat_20 : &lv_font_montserrat_14; }

lv_obj_t* overlay_begin(const char* title)
{
    game_screen_close_overlays();
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, scr_w, scr_h);
    lv_obj_set_style_bg_color(overlay, pal().screen, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(overlay, large ? 16 : 10, 0);
    lv_obj_set_style_pad_row(overlay, large ? 10 : 6, 0);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_clickable(overlay, true);      // swallow taps behind it
    lv_obj_set_scrollable(overlay, false);

    lv_obj_t* t = lv_label_create(overlay);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, pal().ink, 0);
    return overlay;
}

lv_obj_t* overlay_text(const char* s, bool muted)
{
    lv_obj_t* l = lv_label_create(overlay);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_text(l, s);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, muted ? pal().muted : pal().ink, 0);
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

// A button pinned to the bottom of the overlay (outside the column flow)
lv_obj_t* overlay_bottom_button(const char* text, lv_event_cb_t cb, intptr_t user)
{
    lv_obj_t* b = make_key(overlay, lv_pct(100), menu_btn_h(), cb, user);
    lv_obj_add_state(b, LV_STATE_CHECKED);
    key_label(b, text, menu_font());
    lv_obj_set_ignore_layout(b, true);
    lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, 0);
    return b;
}

void start_new(sudoku::Difficulty d)
{
    // Leaving an unfinished puzzle that was actually played counts as giving up
    if (H.record_stat && G->active() && !G->solved() && (G->can_undo() || G->elapsed_s() >= 30)) {
        stats::Record r;
        r.difficulty = static_cast<uint8_t>(G->difficulty());
        r.result = stats::Result::GaveUp;
        r.seconds = G->elapsed_s();
        r.hints = static_cast<uint8_t>(G->hints_used());
        H.record_stat(r);
    }

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
    hint_cell = -1;
    solved_seen = false;
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
            hint_cell = -1;
            solved_seen = false;
            game_screen_close_overlays();
            changed();
            update();
            break;
        case 1: game_screen_open_settings(); break;
        case 2: game_screen_close_overlays(); update(); break;
        case 3: game_screen_open_menu(); break;                // back to the menu
        case 4: if (H.toggle_invert) H.toggle_invert(); break;
        case 5: if (H.toggle_swap_rb) H.toggle_swap_rb(); break;
        case 6:
            if (H.save) H.save(*G);
            if (H.recalibrate_touch) H.recalibrate_touch();
            break;
        case 7: game_screen_open_touch_test(); break;
        case 8:
            game_screen_set_theme(S.theme == Theme::Dark ? Theme::Light : Theme::Dark);
            save_settings();
            game_screen_open_settings();
            break;
        case 9: game_screen_open_stats(); break;
    }
}

// ---- Stats screen -------------------------------------------------------------
// Each table is 4 column labels holding all rows ("\n"-separated) plus a
// header row: 8 objects per table however many rows it has, which keeps LVGL's
// small memory pool safe.
struct StatsTable {
    char   col[4][256];
    size_t len[4];
    int    rows;
};

void stats_table_add(StatsTable& t, const char* a, const char* b, const char* c, const char* d)
{
    // Every column gets a line per row, even when the cell is empty, so the
    // columns stay lined up.
    const char* cells[4] = {a, b, c, d};
    for (int k = 0; k < 4; ++k) {
        const size_t room = sizeof t.col[k] - t.len[k];
        const int n = snprintf(t.col[k] + t.len[k], room, "%s%s", t.rows ? "\n" : "", cells[k]);
        if (n > 0 && size_t(n) < room) t.len[k] += n;
    }
    ++t.rows;
}

void stats_table_show(const StatsTable& t, const char* const head[4],
                      const lv_font_t* head_font, const lv_font_t* body_font)
{
    static const int8_t pct_large[4] = {34, 22, 24, 20};
    static const int8_t pct_small[4] = {30, 29, 24, 17};
    const int8_t* pct = large ? pct_large : pct_small;
    for (int part = 0; part < 2; ++part) {
        lv_obj_t* row = lv_obj_create(overlay);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_clickable(row, false);
        lv_obj_set_scrollable(row, false);
        if (part == 1) lv_obj_set_style_margin_top(row, large ? -6 : -4, 0);  // header hugs its rows
        for (int k = 0; k < 4; ++k) {
            lv_obj_t* l = lv_label_create(row);
            lv_label_set_text(l, part == 0 ? head[k] : t.col[k]);
            lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
            lv_obj_set_width(l, lv_pct(pct[k]));
            lv_obj_set_style_text_font(l, part == 0 ? head_font : body_font, 0);
            lv_obj_set_style_text_color(l, part == 0 ? pal().muted : pal().ink, 0);
            lv_obj_set_style_text_line_space(l, large ? 3 : 1, 0);
        }
    }
}

void stats_back_cb(lv_event_t*) { game_screen_open_menu(); }

} // namespace

// ---- Layout -------------------------------------------------------------------
// Top to bottom: top bar (clock, difficulty, menu), board, tool row (Undo,
// Notes, input mode, Hint), digit row. Sizes come from the screen resolution:
// the board gets the largest cell that leaves the controls their minimum
// height, then any spare height goes back to the controls.
namespace {

lv_obj_t* make_hamburger(lv_obj_t* parent, int w, int h)
{
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_style(b, &st_key_pressed, LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_size(b, w, h);
    lv_obj_set_clickable(b, true);
    lv_obj_set_scrollable(b, false);
    lv_obj_add_event_cb(b, tool_cb, LV_EVENT_CLICKED, reinterpret_cast<void*>(4));

    const int bar_w = h * 3 / 4 < 26 ? h * 3 / 4 : 26;
    const int bar_t = h >= 34 ? 3 : 2;
    const int step  = h >= 34 ? 7 : 5;
    for (int k = -1; k <= 1; ++k) {
        lv_obj_t* bar = lv_obj_create(b);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, bar_w, bar_t);
        lv_obj_set_style_bg_color(bar, pal().ink, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, 1, 0);
        lv_obj_set_clickable(bar, false);
        lv_obj_align(bar, LV_ALIGN_CENTER, 0, k * step);
    }
    return b;
}

void build_layout()
{
    const Palette& P = pal();
    lv_obj_t* scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, P.screen, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);

    scr_w = lv_display_get_horizontal_resolution(lv_display_get_default());
    scr_h = lv_display_get_vertical_resolution(lv_display_get_default());
    large = scr_w >= 300;

    // Small screens drop the digit counters so the board can be bigger
    const bool counters = large;
    const int m = large ? 2 : 1;          // outer margin
    const int g = large ? 4 : 2;          // gap between rows
    int top_h  = large ? 34 : 23;
    int tool_h = large ? 40 : 28;
    int key_h  = large ? 54 : 34;

    const int max_cell_w = (scr_w - 2 * m - 2) / 9;
    const int avail = scr_h - (top_h + tool_h + key_h + 3 * g + 2 * m);
    int cell = (avail - 2) / 9;
    if (cell > max_cell_w) cell = max_cell_w;
    const int board_px = 9 * cell + 2;
    int spare = avail - board_px;
    auto grow = [&spare](int& v, int cap) { const int add = (cap - v) < spare ? (cap - v) : spare; if (add > 0) { v += add; spare -= add; } };
    grow(key_h, 64);
    grow(tool_h, 48);
    grow(top_h, 40);
    const int extra_gap = spare / 4;      // whatever is left: a little air

    // Top bar
    int y = m;
    const lv_font_t* bar_font = large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const int ty = y + (top_h - lv_font_get_line_height(bar_font)) / 2;
    clock_l = lv_label_create(scr);
    lv_obj_set_style_text_font(clock_l, bar_font, 0);
    lv_obj_set_style_text_color(clock_l, P.muted, 0);
    lv_obj_set_pos(clock_l, m + 6, ty);
    center_l = lv_label_create(scr);
    lv_obj_set_style_text_font(center_l, bar_font, 0);
    lv_obj_set_style_text_color(center_l, P.ink, 0);
    lv_obj_align(center_l, LV_ALIGN_TOP_MID, 0, ty);
    const int hb_w = top_h * 3 / 2;
    lv_obj_t* hb = make_hamburger(scr, hb_w, top_h);
    lv_obj_set_pos(hb, scr_w - m - hb_w, y);
    y += top_h + g + extra_gap;

    // Board
    board = board_create(scr, G, cell, on_cell);
    lv_obj_set_pos(board, (scr_w - board_px) / 2, y);
    y += board_px + g + extra_gap;

    // Tool row: Undo | Notes | Cell 1st/Digit 1st | Hint, widths from the
    // labels. Uses the large font only if all four fit with some padding.
    const int row_w = scr_w - 2 * m;
    const int tg = large ? 5 : 3;
    const char* names[4] = {"Undo", "Notes", "Digit 1st", "Hint"};   // [2] = widest mode label
    const lv_font_t* tf = large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    int nat[4], total = 0;
    for (int k = 0; k < 4; ++k) { nat[k] = text_width(names[k], tf); total += nat[k]; }
    if (total + 4 * 12 + 3 * tg > row_w && tf != &lv_font_montserrat_14) {
        tf = &lv_font_montserrat_14;
        total = 0;
        for (int k = 0; k < 4; ++k) { nat[k] = text_width(names[k], tf); total += nat[k]; }
    }
    const int pad_each = (row_w - total - 3 * tg) / 4;   // spread spare width evenly
    lv_obj_t* tb[4];
    int x = m;
    for (int k = 0; k < 4; ++k) {
        const int w = (k == 3) ? (m + row_w - x) : nat[k] + pad_each;
        tb[k] = make_key(scr, w, tool_h, tool_cb, k);
        lv_obj_set_pos(tb[k], x, y);
        lv_obj_t* l = key_label(tb[k], names[k], tf);
        if (k == 2) mode_lbl = l;
        x += w + tg;
    }
    undo_btn = tb[0]; notes_btn = tb[1]; mode_btn = tb[2]; hint_btn = tb[3];
    y += tool_h + g + extra_gap;

    // Digit row (with "left to place" counts on large screens)
    const int dgap = large ? 4 : 2;
    const int key_w = (scr_w - 2 * m - 8 * dgap) / 9;
    const int keys_x = (scr_w - (9 * key_w + 8 * dgap)) / 2;
    const lv_font_t* kf = key_h >= 56 ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    const lv_font_t* cf = &lv_font_montserrat_12;
    const int cnt_h = counters ? lv_font_get_line_height(cf) : 0;
    char s[2] = {0, 0};
    for (int d = 1; d <= 9; ++d) {
        lv_obj_t* k = make_key(scr, key_w, key_h, digit_cb, d);
        lv_obj_set_pos(k, keys_x + (d - 1) * (key_w + dgap), y);
        s[0] = '0' + d;
        lv_obj_t* l = key_label(k, s, kf);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, -cnt_h / 2);
        digit_btn[d] = k;
        digit_cnt[d] = nullptr;
        if (counters) {
            digit_cnt[d] = lv_label_create(k);
            lv_obj_set_style_text_font(digit_cnt[d], cf, 0);
            lv_obj_set_style_text_opa(digit_cnt[d], LV_OPA_80, 0);
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
    lv_obj_set_style_bg_color(d, first ? pal().conflict : pal().entry, 0);
    if (first) lv_obj_move_foreground(d);
}

void tt_finish_tap()
{
    if (tt_n == 0) return;
    // "Settled" position = median of the second half of the readings
    int16_t xs[kMaxSamples], ys[kMaxSamples];
    const int from = tt_n / 2, n = tt_n - from;
    for (int k = 0; k < n; ++k) { xs[k] = tt_x[from + k]; ys[k] = tt_y[from + k]; }
    for (int a = 0; a < n; ++a) for (int b = a + 1; b < n; ++b) {
        if (xs[b] < xs[a]) { int16_t t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
        if (ys[b] < ys[a]) { int16_t t = ys[a]; ys[a] = ys[b]; ys[b] = t; }
    }
    const int sx = xs[n / 2], sy = ys[n / 2];
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
void game_screen_create(game::Game& g, const UiHooks& hooks, const UiSettings& settings)
{
    G = &g;
    H = hooks;
    S = settings;
    set_theme(S.theme);
    apply_styles();
    selected = -1;
    notes_mode = false;
    brush_digit = 0;
    hint_cell = -1;
    solved_seen = g.solved();      // a finished saved game isn't celebrated again
    overlay = nullptr;
    build_layout();
    update();
}

void game_screen_set_theme(Theme t)
{
    S.theme = t;
    set_theme(t);
    apply_styles();
    build_layout();
    update();
}

void game_screen_tick(uint32_t now_ms)
{
    now_cache = now_ms;
    if (!G) return;
    // Idle pause: LVGL tracks the time since the last touch
    const bool idle = lv_display_get_inactive_time(lv_display_get_default()) >= kIdlePauseMs;
    if (idle != idle_paused) {
        idle_paused = idle;
        update_status();
    }
    if (last_sec_ms == 0) last_sec_ms = now_ms;
    if (now_ms - last_sec_ms >= 1000) {
        last_sec_ms += 1000;
        if (!overlay && !idle_paused && G->active() && !G->solved()) {
            G->add_second();
            update_status();
        }
    }
    // Save 1.5 s after the last move, and every 30 s of play for the clock.
    const bool due = (dirty && now_ms - dirty_ms >= 1500)
                  || (now_ms - last_save_ms >= 30000 && !idle_paused && G->active() && !G->solved());
    if (due && H.save) {
        H.save(*G);
        dirty = false;
        last_save_ms = now_ms;
    }
}

void game_screen_tap_cell(int idx)            { on_cell(idx); }
void game_screen_tap_digit(int d)             { on_digit(d); }
void game_screen_set_notes(bool on)           { notes_mode = on; update(); }
void game_screen_hint()                       { on_hint(); }
void game_screen_set_input_mode(InputMode m)
{
    if (S.input != m) toggle_input_mode();
    update();
}

void game_screen_open_menu()
{
    overlay_begin("CYD Sudoku");
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
    if (H.load_stats) overlay_button(overlay, "Stats", menu_cb, 9);
    overlay_button(overlay, "Display & touch", menu_cb, 1);
    overlay_button(overlay, "Back to game", menu_cb, 2, true);
}

void game_screen_open_settings()
{
    overlay_begin("Display & touch");
    char theme_txt[32];
    snprintf(theme_txt, sizeof theme_txt, "Theme: %s", theme_name(S.theme));
    overlay_button(overlay, theme_txt, menu_cb, 8);
    overlay_button(overlay, "Invert panel colors", menu_cb, 4);
    overlay_button(overlay, "Swap red and blue", menu_cb, 5);
    overlay_button(overlay, "Recalibrate touch", menu_cb, 6);
    if (H.raw_touch) overlay_button(overlay, "Touch test", menu_cb, 7);
    overlay_button(overlay, "Back", menu_cb, 3, true);
    char info[96];
    snprintf(info, sizeof info, "%s, firmware %s", H.board_name ? H.board_name : "",
             H.firmware_version ? H.firmware_version : "");
    overlay_text(info, true);
}

void game_screen_open_stats()
{
    overlay_begin("Stats");
    static stats::Summary sum;               // ~200 bytes; static keeps it off the stack
    const bool ok = H.load_stats && H.load_stats(sum);
    const lv_font_t* tf = large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const lv_font_t* rf = &lv_font_montserrat_14;

    if (!ok || sum.total == 0) {
        overlay_text("No games recorded yet. Solved puzzles, and puzzles you leave "
                     "for a new game, are listed here.", false);
    } else {
        static StatsTable levels, recent;           // ~2 KB each: keep off the stack
        levels = StatsTable{};
        recent = StatsTable{};
        for (int d = 0; d < 4; ++d) {
            const stats::PerDifficulty& p = sum.level[d];
            char n[12], avg[16] = "-", best[16] = "-";
            snprintf(n, sizeof n, "%lu", (unsigned long)p.solved);
            if (p.solved) {
                stats::format_time(avg, sizeof avg, sum.average_s(d));
                stats::format_time(best, sizeof best, p.best_s);
            }
            stats_table_add(levels, sudoku::difficulty_name(static_cast<sudoku::Difficulty>(d)), n, avg, best);
        }
        const char* const head1[4] = {"Level", "Solved", large ? "Average" : "Avg", "Best"};
        stats_table_show(levels, head1, rf, tf);

        const int max_rows = large ? 7 : 5;
        const int show = sum.recent_n < max_rows ? sum.recent_n : max_rows;
        for (int i = 0; i < show; ++i) {
            const stats::Record& r = sum.newest(i);
            char t[16], h[8] = "";
            stats::format_time(t, sizeof t, r.seconds);
            if (r.hints) snprintf(h, sizeof h, "%u", (unsigned)r.hints);
            stats_table_add(recent, sudoku::difficulty_name(static_cast<sudoku::Difficulty>(r.difficulty & 3)),
                            stats::result_name(r.result), t, h);
        }
        const char* const head2[4] = {"Recent", "", "Time", "Hints"};
        stats_table_show(recent, head2, rf, rf);
    }
    char where[64];
    snprintf(where, sizeof where, "Saved on the %s.",
             H.stats_location ? H.stats_location() : "board");
    overlay_text(where, true);
    overlay_bottom_button("Back", stats_back_cb, 0);
}

void game_screen_open_touch_test()
{
    overlay_begin("Touch test");
    tt_info = overlay_text("Tap anywhere. Red dot = first reading of a tap, blue = the rest.", true);
    tt_n = tt_misses = tt_log_n = tt_dot_next = 0;
    for (auto& d : tt_dots) d = nullptr;
    overlay_bottom_button("Done", tt_close_cb, 0);
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
