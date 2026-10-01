#include "sudoku.h"

namespace sudoku {

namespace {

// Bitmask solver: bit d (1..9) set in a row/col/box mask = digit used there.
struct Solver {
    uint8_t  cell[N];
    uint16_t row[9], col[9], box[9];
    int      solutions = 0;
    int      limit     = 2;
    Grid*    out       = nullptr;
    Rng*     rng       = nullptr;   // when set, digits are tried in random order

    bool load(const Grid& g)
    {
        for (int i = 0; i < 9; ++i) row[i] = col[i] = box[i] = 0;
        for (int i = 0; i < N; ++i) {
            cell[i] = g.c[i];
            if (!g.c[i]) continue;
            const uint16_t bit = 1u << g.c[i];
            const int r = row_of(i), c = col_of(i), b = box_of(i);
            if ((row[r] | col[c] | box[b]) & bit) return false;  // givens conflict
            row[r] |= bit; col[c] |= bit; box[b] |= bit;
        }
        return true;
    }

    void search()
    {
        // Pick the empty cell with the fewest candidates (MRV).
        int best = -1, best_count = 10;
        uint16_t best_cand = 0;
        for (int i = 0; i < N; ++i) {
            if (cell[i]) continue;
            const uint16_t cand = ~(row[row_of(i)] | col[col_of(i)] | box[box_of(i)]) & 0x3FE;
            const int n = __builtin_popcount(cand);
            if (n < best_count) {
                best = i; best_count = n; best_cand = cand;
                if (n <= 1) break;
            }
        }
        if (best < 0) {                       // grid full: a solution
            if (solutions == 0 && out) for (int i = 0; i < N; ++i) out->c[i] = cell[i];
            ++solutions;
            return;
        }
        if (best_count == 0) return;          // dead end

        uint8_t order[9];
        int n = 0;
        for (int d = 1; d <= 9; ++d) if (best_cand & (1u << d)) order[n++] = d;
        if (rng) for (int k = n - 1; k > 0; --k) {
            const int j = rng->below(k + 1);
            const uint8_t t = order[k]; order[k] = order[j]; order[j] = t;
        }

        const int r = row_of(best), c = col_of(best), b = box_of(best);
        for (int k = 0; k < n && solutions < limit; ++k) {
            const uint16_t bit = 1u << order[k];
            cell[best] = order[k];
            row[r] |= bit; col[c] |= bit; box[b] |= bit;
            search();
            row[r] &= ~bit; col[c] &= ~bit; box[b] &= ~bit;
        }
        cell[best] = 0;
    }
};

int target_clues(Difficulty d)
{
    switch (d) {
        case Difficulty::Easy:   return 38;
        case Difficulty::Medium: return 32;
        case Difficulty::Hard:   return 28;
        default:                 return 17;   // Expert: dig as far as possible
    }
}

} // namespace

const char* difficulty_name(Difficulty d)
{
    switch (d) {
        case Difficulty::Easy:   return "Easy";
        case Difficulty::Medium: return "Medium";
        case Difficulty::Hard:   return "Hard";
        default:                 return "Expert";
    }
}

int count_solutions(const Grid& g, int limit, Grid* out)
{
    Solver s;
    if (!s.load(g)) return 0;
    s.limit = limit;
    s.out = out;
    s.search();
    return s.solutions;
}

int clue_count(const Grid& g)
{
    int n = 0;
    for (int i = 0; i < N; ++i) n += g.c[i] != 0;
    return n;
}

namespace {

// One attempt: random full grid, then dig toward the clue target.
void generate_once(int target, Rng& rng, Grid& puzzle, Grid& solution)
{
    // 1. A random complete grid: solve an empty grid trying digits randomly.
    Grid empty{};
    Solver s;
    s.load(empty);
    s.limit = 1;
    s.out = &solution;
    s.rng = &rng;
    s.search();

    // 2. Dig holes in symmetric pairs, in random order, keeping uniqueness.
    puzzle = solution;
    uint8_t order[41];                     // cells 0..40 (40 is the centre)
    for (int i = 0; i <= 40; ++i) order[i] = i;
    for (int k = 40; k > 0; --k) {
        const int j = rng.below(k + 1);
        const uint8_t t = order[k]; order[k] = order[j]; order[j] = t;
    }

    int clues = N;
    for (int k = 0; k <= 40 && clues > target; ++k) {
        const int a = order[k], b = 80 - a;
        const uint8_t va = puzzle.c[a], vb = puzzle.c[b];
        puzzle.c[a] = 0; puzzle.c[b] = 0;
        if (count_solutions(puzzle, 2) == 1) {
            clues -= (a == b) ? 1 : 2;
        } else {
            puzzle.c[a] = va; puzzle.c[b] = vb;   // removal broke uniqueness
        }
    }
}

} // namespace

void generate(Difficulty d, Rng& rng, Grid& puzzle, Grid& solution)
{
    // Digging order decides how far a grid can be thinned, so harder levels
    // try several grids and keep the one with the fewest clues.
    const int target   = target_clues(d);
    const int attempts = (d == Difficulty::Expert) ? 12 : (d == Difficulty::Hard) ? 6 : 1;
    int best = N + 1;
    for (int a = 0; a < attempts; ++a) {
        Grid p, s;
        generate_once(target, rng, p, s);
        const int c = clue_count(p);
        if (c < best) { best = c; puzzle = p; solution = s; }
        if (d != Difficulty::Expert && best <= target) break;
    }
}

} // namespace sudoku
