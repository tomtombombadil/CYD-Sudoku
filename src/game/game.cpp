#include "game.h"

#include <cstring>

namespace game {

using sudoku::same_unit;

void Game::start(Difficulty d, sudoku::Rng& rng)
{
    diff_ = d;
    sudoku::generate(d, rng, puzzle_, solution_);
    restart();
}

void Game::restart()
{
    for (int i = 0; i < N; ++i) { value_[i] = puzzle_.c[i]; notes_[i] = 0; }
    elapsed_s_ = 0;
    undo_n_ = 0;
    group_ = 0;
    active_ = true;
}

bool Game::conflict(int i) const
{
    const uint8_t v = value_[i];
    if (!v) return false;
    for (int j = 0; j < N; ++j)
        if (j != i && value_[j] == v && same_unit(i, j)) return true;
    return false;
}

int Game::placed_correct(int d) const
{
    int n = 0;
    for (int i = 0; i < N; ++i) n += (value_[i] == d && solution_.c[i] == d);
    return n;
}

int Game::count(int d) const
{
    int n = 0;
    for (int i = 0; i < N; ++i) n += (value_[i] == d);
    return n;
}

bool Game::solved() const
{
    if (!active_) return false;
    for (int i = 0; i < N; ++i) if (value_[i] != solution_.c[i]) return false;
    return true;
}

void Game::record(int i)
{
    if (undo_n_ == kUndoCap) {
        // Full: drop the oldest quarter. Groups split at the cut are only a
        // problem for actions nobody can reach by undo any more.
        constexpr int drop = kUndoCap / 4;
        memmove(undo_, undo_ + drop, (kUndoCap - drop) * sizeof(Change));
        undo_n_ -= drop;
    }
    undo_[undo_n_++] = Change{static_cast<uint8_t>(i), value_[i], notes_[i], group_};
}

bool Game::enter(int i, int d, bool notes_mode)
{
    if (!active_ || i < 0 || i >= N || d < 1 || d > 9 || given(i)) return false;

    if (notes_mode) {
        if (value_[i]) return false;            // notes only on empty cells
        begin_action();
        record(i);
        notes_[i] ^= (1u << d);
        return true;
    }

    begin_action();
    record(i);
    if (value_[i] == d) {                       // same digit again: clear it
        value_[i] = 0;
        return true;
    }
    value_[i] = d;
    notes_[i] = 0;
    // Auto-clear: d can no longer be a candidate anywhere it now sees.
    const uint16_t bit = 1u << d;
    for (int j = 0; j < N; ++j) {
        if (j != i && (notes_[j] & bit) && same_unit(i, j)) {
            record(j);
            notes_[j] &= ~bit;
        }
    }
    return true;
}

bool Game::erase(int i)
{
    if (!active_ || i < 0 || i >= N || given(i)) return false;
    if (!value_[i] && !notes_[i]) return false;
    begin_action();
    record(i);
    value_[i] = 0;
    notes_[i] = 0;
    return true;
}

bool Game::undo()
{
    if (!undo_n_) return false;
    const uint16_t g = undo_[undo_n_ - 1].group;
    while (undo_n_ && undo_[undo_n_ - 1].group == g) {
        const Change& c = undo_[--undo_n_];
        value_[c.idx] = c.value;
        notes_[c.idx] = c.notes;
    }
    return true;
}

// ---- Save layout (little-endian, version 1) --------------------------------
//   u32 magic 'SUD1' | u8 difficulty | u8 active | u16 undo_n | u16 group
//   u32 elapsed_s | puzzle[81] | solution[81] | value[81] | notes u16[81]
//   Change[undo_n] (6 bytes each: idx, value, notes u16, group u16)
namespace {
constexpr uint32_t kMagic = 0x31445553;  // "SUD1"
constexpr size_t   kHeader = 4 + 1 + 1 + 2 + 2 + 4;
constexpr size_t   kBody = 81 * 3 + 81 * 2;

template <class T> void put(uint8_t*& p, T v) { memcpy(p, &v, sizeof v); p += sizeof v; }
template <class T> T get(const uint8_t*& p) { T v; memcpy(&v, p, sizeof v); p += sizeof v; return v; }
} // namespace

size_t Game::max_serialized_size() { return kHeader + kBody + kUndoCap * 6; }

size_t Game::serialize(uint8_t* buf, size_t cap) const
{
    const size_t need = kHeader + kBody + size_t(undo_n_) * 6;
    if (cap < need) return 0;
    uint8_t* p = buf;
    put<uint32_t>(p, kMagic);
    put<uint8_t>(p, static_cast<uint8_t>(diff_));
    put<uint8_t>(p, active_ ? 1 : 0);
    put<uint16_t>(p, undo_n_);
    put<uint16_t>(p, group_);
    put<uint32_t>(p, elapsed_s_);
    memcpy(p, puzzle_.c, 81);   p += 81;
    memcpy(p, solution_.c, 81); p += 81;
    memcpy(p, value_, 81);      p += 81;
    for (int i = 0; i < N; ++i) put<uint16_t>(p, notes_[i]);
    for (int k = 0; k < undo_n_; ++k) {
        put<uint8_t>(p, undo_[k].idx);
        put<uint8_t>(p, undo_[k].value);
        put<uint16_t>(p, undo_[k].notes);
        put<uint16_t>(p, undo_[k].group);
    }
    return need;
}

bool Game::deserialize(const uint8_t* buf, size_t len)
{
    if (len < kHeader + kBody) return false;
    const uint8_t* p = buf;
    if (get<uint32_t>(p) != kMagic) return false;
    const uint8_t d = get<uint8_t>(p);
    const uint8_t act = get<uint8_t>(p);
    const uint16_t un = get<uint16_t>(p);
    const uint16_t grp = get<uint16_t>(p);
    const uint32_t el = get<uint32_t>(p);
    if (d > 3 || un > kUndoCap || len < kHeader + kBody + size_t(un) * 6) return false;

    // Validate into a scratch copy, then commit. Static: a Game is ~4 KB,
    // too big for the ESP32's default task stack.
    static Game g;
    g.diff_ = static_cast<Difficulty>(d);
    g.active_ = act != 0;
    g.undo_n_ = un;
    g.group_ = grp;
    g.elapsed_s_ = el;
    memcpy(g.puzzle_.c, p, 81);   p += 81;
    memcpy(g.solution_.c, p, 81); p += 81;
    memcpy(g.value_, p, 81);      p += 81;
    for (int i = 0; i < N; ++i) {
        g.notes_[i] = get<uint16_t>(p) & 0x3FE;
        if (g.value_[i] > 9 || g.puzzle_.c[i] > 9 || g.solution_.c[i] < 1 || g.solution_.c[i] > 9)
            return false;
        if (g.puzzle_.c[i] && g.puzzle_.c[i] != g.solution_.c[i]) return false;
    }
    for (int k = 0; k < un; ++k) {
        Change c;
        c.idx = get<uint8_t>(p);
        c.value = get<uint8_t>(p);
        c.notes = get<uint16_t>(p);
        c.group = get<uint16_t>(p);
        if (c.idx >= N || c.value > 9) return false;
        g.undo_[k] = c;
    }
    *this = g;
    return true;
}

} // namespace game
