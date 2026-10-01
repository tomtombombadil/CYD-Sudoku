// Saves the player's UI settings (theme, input mode) to LittleFS.
#pragma once

#include "ui/theme.h"

ui::UiSettings settings_store_load();   // defaults if nothing saved
void           settings_store_save(const ui::UiSettings& s);
