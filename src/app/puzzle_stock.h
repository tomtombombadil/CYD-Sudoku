// Ready-made puzzles, so "New game" is instant.
//
// Grading by solving technique (src/game/grader.*) means Hard and Expert can
// take a few seconds to find on the ESP32. A background task on the second
// CPU core keeps two puzzles per difficulty ready and saves them to LittleFS
// (/puzzle_stock.bin) so they survive power-off.
#pragma once

#include "game/sudoku.h"

void puzzle_stock_begin();            // load saved stock, start the background task
// Take a ready puzzle; false if none is ready (caller generates one itself).
bool puzzle_stock_take(sudoku::Difficulty d, sudoku::Grid& puzzle, sudoku::Grid& solution);
void puzzle_stock_loop();             // call from loop(): saves the stock when it changed
