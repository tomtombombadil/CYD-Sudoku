#include "save_store.h"

#include <Arduino.h>
#include <LittleFS.h>
#include "hal/storage.h"

namespace {
constexpr const char* kPath = "/game.bin";
constexpr const char* kTmp  = "/game.tmp";
uint8_t buf[game::Game::kUndoCap * 6 + 512];   // >= Game::max_serialized_size()
}

bool save_store_load(game::Game& g)
{
    static_assert(sizeof(buf) >= 4 + 1 + 1 + 2 + 2 + 4 + 81 * 3 + 81 * 2 + game::Game::kUndoCap * 6,
                  "save buffer too small");
    if (!storage_begin() || !LittleFS.exists(kPath)) return false;
    File f = LittleFS.open(kPath, "r");
    if (!f) return false;
    const size_t n = f.read(buf, sizeof buf);
    f.close();
    const bool ok = g.deserialize(buf, n);
    Serial.printf("[save] load %s (%u bytes)\n", ok ? "ok" : "rejected", (unsigned)n);
    return ok;
}

void save_store_save(const game::Game& g)
{
    if (!storage_begin()) return;
    const size_t n = g.serialize(buf, sizeof buf);
    if (!n) return;
    // Write a temp file, then rename over the old save, so a power cut
    // mid-write never leaves a half-written game.
    File f = LittleFS.open(kTmp, "w");
    if (!f) return;
    const size_t w = f.write(buf, n);
    f.close();
    if (w != n) { LittleFS.remove(kTmp); return; }
    LittleFS.rename(kTmp, kPath);   // LittleFS rename replaces atomically
}
