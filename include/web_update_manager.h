#pragma once
#include <Arduino.h>
#include "web_slot_manager.h"
#include "web_manifest.h"
#include "web_update_space.h"
namespace WebUpdateManager {
constexpr uint32_t TimeoutMs = 120000;
struct Snapshot {
    uint32_t id;
    WebSlotManager::Slot source, target;
    uint32_t generation, lastActivity, totalBytes;
    uint16_t fileCount;
    char version[33];
    const char* lastError;
};
bool active();
Snapshot snapshot();
struct Preflight {
    WebUpdateSpace::Estimate space;
    const char* error;
};
// Requires a Result from successful WebManifest::validate. No filesystem writes.
Preflight preflight(const WebManifest::Result& manifest);
bool start(const WebManifest::Result& manifest, uint32_t& id);
bool touch(uint32_t id);
bool cancel(uint32_t id);
void update();
}
