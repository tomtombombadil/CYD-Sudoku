#include "storage.h"

#include <Arduino.h>
#include <LittleFS.h>

bool storage_begin()
{
    static bool tried = false;
    static bool ok    = false;
    if (!tried) {
        tried = true;
        ok = LittleFS.begin(true);   // true = format if never formatted
        if (!ok) Serial.println("[storage] LittleFS mount failed - using defaults");
    }
    return ok;
}
