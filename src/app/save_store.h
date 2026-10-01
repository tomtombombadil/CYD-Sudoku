// Saves the current game to LittleFS so it resumes after a power cycle.
#pragma once

#include "game/game.h"

bool save_store_load(game::Game& g);    // false if no valid save exists
void save_store_save(const game::Game& g);
