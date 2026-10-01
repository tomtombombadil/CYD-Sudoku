#include "stats_store.h"

#include <Arduino.h>
#include <LittleFS.h>
#include "hal/sdcard.h"
#include "hal/storage.h"

namespace {

constexpr const char* kSdDir      = "/CYD-Sudoku";
constexpr const char* kSdPath     = "/CYD-Sudoku/stats.csv";
constexpr const char* kFlashPath  = "/stats.csv";
constexpr uint32_t    kFlashKeep  = 250;   // records kept on flash after a trim
constexpr uint32_t    kFlashLimit = 300;   // trim when the flash file passes this

bool on_sd = false;

// Calls fn(line) for every line of the file (without the newline).
template <class Fn>
bool for_each_line(fs::FS& fs, const char* path, Fn fn)
{
    File f = fs.open(path, "r");
    if (!f) return false;
    char chunk[256], line[96];
    size_t len = 0;
    for (;;) {
        const int n = f.read(reinterpret_cast<uint8_t*>(chunk), sizeof chunk);
        if (n <= 0) break;
        for (int k = 0; k < n; ++k) {
            const char c = chunk[k];
            if (c == '\n' || c == '\r') {
                if (len) { line[len] = 0; fn(line); len = 0; }
            } else if (len < sizeof line - 1) {
                line[len++] = c;
            }
        }
    }
    if (len) { line[len] = 0; fn(line); }
    f.close();
    return true;
}

uint32_t count_records(fs::FS& fs, const char* path)
{
    uint32_t n = 0;
    stats::Record r;
    for_each_line(fs, path, [&](const char* l) { if (stats::parse_line(l, r)) ++n; });
    return n;
}

bool append_line(fs::FS& fs, const char* path, const char* text)
{
    const bool fresh = !fs.exists(path);
    File f = fs.open(path, "a");
    if (!f) return false;
    bool ok = true;
    if (fresh) ok = f.print(stats::kCsvHeader) > 0;
    ok = ok && f.print(text) > 0;
    f.close();
    return ok;
}

// Move records saved on flash onto the card, renumbering them after the
// card's own records, then delete the flash copy.
void migrate_flash_to_sd()
{
    if (!storage_begin() || !LittleFS.exists(kFlashPath)) return;
    fs::FS& sd = sd_fs();
    uint32_t seq = count_records(sd, kSdPath);
    bool ok = true;
    char out[96];
    stats::Record r;
    for_each_line(LittleFS, kFlashPath, [&](const char* l) {
        if (ok && stats::parse_line(l, r) && stats::format_line(out, sizeof out, ++seq, r))
            ok = append_line(sd, kSdPath, out);
    });
    if (ok) {
        LittleFS.remove(kFlashPath);
        Serial.println("[stats] moved flash records to SD card");
    }
}

// Picks the card if there is one, else flash. Returns the file system.
fs::FS* target(const char** path)
{
    if (sd_begin()) {
        fs::FS& sd = sd_fs();
        if (!sd.exists(kSdDir)) sd.mkdir(kSdDir);
        if (!on_sd) migrate_flash_to_sd();
        on_sd = true;
        *path = kSdPath;
        return &sd;
    }
    on_sd = false;
    if (!storage_begin()) return nullptr;
    *path = kFlashPath;
    return &LittleFS;
}

// Keep the flash file small: rewrite it with only the newest records.
void trim_flash(uint32_t total)
{
    if (total <= kFlashLimit) return;
    const char* tmp = "/stats.tmp";
    File out = LittleFS.open(tmp, "w");
    if (!out) return;
    out.print(stats::kCsvHeader);
    uint32_t seen = 0, kept = 0;
    char line_out[96];
    stats::Record r;
    for_each_line(LittleFS, kFlashPath, [&](const char* l) {
        if (!stats::parse_line(l, r)) return;
        if (++seen > total - kFlashKeep && stats::format_line(line_out, sizeof line_out, ++kept, r))
            out.print(line_out);
    });
    out.close();
    LittleFS.rename(tmp, kFlashPath);
}

} // namespace

void stats_store_record(const stats::Record& r)
{
    const char* path = nullptr;
    fs::FS* fs = target(&path);
    if (!fs) return;
    const uint32_t seq = count_records(*fs, path) + 1;
    char line[96];
    if (!stats::format_line(line, sizeof line, seq, r)) return;
    if (!append_line(*fs, path, line)) {
        if (on_sd) {                       // card pulled? fall back to flash
            sd_lost();
            on_sd = false;
            if (storage_begin()) append_line(LittleFS, kFlashPath, line);
        }
        return;
    }
    if (!on_sd) trim_flash(seq);
    Serial.printf("[stats] %s", line);
}

bool stats_store_load(stats::Summary& out)
{
    out = stats::Summary{};
    const char* path = nullptr;
    fs::FS* fs = target(&path);
    if (!fs) return false;
    stats::Record r;
    return for_each_line(*fs, path, [&](const char* l) { if (stats::parse_line(l, r)) out.add(r); });
}

const char* stats_store_location() { return on_sd ? "SD card" : "board memory"; }
