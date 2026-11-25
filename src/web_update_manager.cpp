#include "web_update_manager.h"
#include <cstring>
#include <Updater.h>
#include "firmware_update.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "storage.h"
namespace WebUpdateManager {
namespace {
Snapshot session{};
uint32_t sequence = 0;
void clear(const char* error) {
    session = Snapshot{};
    session.lastError = error;
}
}
bool active() { return session.id != 0; }
Snapshot snapshot() { return session; }
void update() {
    if (!active()) return;
    if (session.source != WebSlotManager::activeSlot() ||
        session.generation != WebSlotManager::selectionGeneration()) {
        clear("slot-changed");
    } else if (uint32_t(millis() - session.lastActivity) >= TimeoutMs) {
        clear("timeout");
    }
}
bool start(const WebManifest::Result& manifest, uint32_t& id) {
    update();
    id = 0;
    if (FirmwareUpdate::active() || Update.isRunning() || WebServer::maintenancePending() ||
        !Storage::isReady() || !WifiManager::isConnected() || WifiManager::isPortalActive()) return false;
    if (active() || manifest.error || manifest.fileCount != 12 ||
        !manifest.totalBytes || manifest.totalBytes > 384UL * 1024 ||
        !manifest.version[0] || !memchr(manifest.version, 0, sizeof(manifest.version))) return false;
    clear(nullptr);
    if (++sequence == 0) ++sequence;
    session.id = sequence;
    session.source = WebSlotManager::activeSlot();
    session.target = WebSlotManager::inactiveSlot();
    session.generation = WebSlotManager::selectionGeneration();
    session.lastActivity = millis();
    session.totalBytes = manifest.totalBytes;
    session.fileCount = manifest.fileCount;
    memcpy(session.version, manifest.version, sizeof(session.version));
    id = session.id;
    return true;
}
bool touch(uint32_t id) {
    update();
    if (!id || id != session.id) return false;
    session.lastActivity = millis();
    return true;
}
bool cancel(uint32_t id) {
    update();
    if (!id || id != session.id) return false;
    clear("cancelled");
    return true;
}
}
