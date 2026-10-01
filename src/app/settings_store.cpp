#include "settings_store.h"

#include <Arduino.h>
#include <LittleFS.h>
#include "hal/storage.h"

namespace {
constexpr const char* kPath  = "/ui_settings.bin";
constexpr uint32_t    kMagic = 0x31534955;  // "UIS1"

struct SettingsFile {
    uint32_t magic;
    uint8_t  theme;
    uint8_t  input;
    uint8_t  reserved[6];   // room for later settings without a format change
};
} // namespace

ui::UiSettings settings_store_load()
{
    ui::UiSettings s;
    if (!storage_begin() || !LittleFS.exists(kPath)) return s;
    File f = LittleFS.open(kPath, "r");
    SettingsFile d{};
    if (f && f.read(reinterpret_cast<uint8_t*>(&d), sizeof d) == sizeof d && d.magic == kMagic) {
        if (d.theme <= 1) s.theme = static_cast<ui::Theme>(d.theme);
        if (d.input <= 1) s.input = static_cast<ui::InputMode>(d.input);
    }
    f.close();
    return s;
}

void settings_store_save(const ui::UiSettings& s)
{
    if (!storage_begin()) return;
    SettingsFile d{kMagic, static_cast<uint8_t>(s.theme), static_cast<uint8_t>(s.input), {}};
    File f = LittleFS.open(kPath, "w");
    if (!f) return;
    f.write(reinterpret_cast<const uint8_t*>(&d), sizeof d);
    f.close();
}
