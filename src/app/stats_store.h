// Saves play history (stats) as CSV.
//
// With a usable SD card: /CYD-Sudoku/stats.csv on the card, full history,
// opens in Excel. Without one: /stats.csv on the board's flash, the most
// recent records only. Records made on flash are moved to the card the
// first time a card is found.
#pragma once

#include "game/stats.h"

void        stats_store_record(const stats::Record& r);
// Fills `out` from the whole saved history. False if nothing could be read.
bool        stats_store_load(stats::Summary& out);
// Remove the most recent record (e.g. a game recorded by mistake).
bool        stats_store_delete_last();
// Remove all records (card and board memory).
bool        stats_store_clear();
// Where the last load/record went: "SD card" or "board memory".
const char* stats_store_location();
