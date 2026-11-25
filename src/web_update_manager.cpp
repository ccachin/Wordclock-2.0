#include "web_update_manager.h"
#include <cstring>
#include <bearssl/bearssl_hash.h>
#include <Updater.h>
#include <LittleFS.h>
#include "firmware_update.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "storage.h"
namespace WebUpdateManager {
namespace {
Snapshot session{};
uint32_t sequence = 0;
uint32_t sizes[WebManifest::AssetCount]{}, offsets[WebManifest::AssetCount]{};
uint8_t hashes[WebManifest::AssetCount][32]{};
bool manifestStored = false;
String pathFor(const char* name) { return String(WebSlotManager::root(session.target))+"/"+name; }
bool cleanTarget() {
    if (session.target == WebSlotManager::activeSlot() || !WebSlotManager::isIncomplete(session.target)) return false;
    for (size_t i=0;i<WebManifest::AssetCount;++i) {
        String path=pathFor(WebManifest::assetName(i));
        if (LittleFS.exists(path) && !LittleFS.remove(path)) return false;
        path += ".tmp";
        if (LittleFS.exists(path) && !LittleFS.remove(path)) return false;
    }
    for (const char* name : {"web-manifest.json", ".development"}) {
        String path=pathFor(name);
        if (LittleFS.exists(path) && !LittleFS.remove(path)) return false;
    }
    return true;
}
void clear(const char* error) {
    session = Snapshot{};
    manifestStored=false;
    memset(offsets,0,sizeof(offsets));
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
        cleanTarget();
        clear("timeout");
    }
}
Preflight preflight(const WebManifest::Result& manifest) {
    Preflight result{};
    if (!Storage::isReady()) result.error = "storage-unavailable";
    else if (active() || FirmwareUpdate::active() || Update.isRunning() ||
             WebServer::maintenancePending()) result.error = "operation-active";
    else if (!WifiManager::isConnected() || WifiManager::isPortalActive())
        result.error = "wifi-unavailable";
    else if (manifest.error || manifest.fileCount != 12 ||
             !manifest.totalBytes || manifest.totalBytes > 384UL * 1024 ||
             !manifest.version[0] || !memchr(manifest.version, 0, sizeof(manifest.version)))
        result.error = "invalid-manifest";
    else if (WebSlotManager::activeSlot() == WebSlotManager::inactiveSlot() ||
             !WebSlotManager::hasEntryPoint(WebSlotManager::activeSlot()))
        result.error = "slot-unavailable";
    else {
        FSInfo info{};
        if (!LittleFS.info(info)) result.error = "storage-info";
        else {
            result.space = WebUpdateSpace::estimate(info.totalBytes, info.usedBytes, manifest.totalBytes);
            result.error = result.space.error;
        }
    }
    return result;
}
bool start(const WebManifest::Result& manifest, uint32_t& id) {
    update();
    id = 0;
    if (preflight(manifest).error) return false;
    if (!WebSlotManager::markIncomplete(WebSlotManager::inactiveSlot())) {
        clear("slot-protection-failed");
        return false;
    }
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
    memcpy(sizes,manifest.sizes,sizeof(sizes));
    memcpy(hashes,manifest.hashes,sizeof(hashes));
    if (!cleanTarget()) { clear("cleanup-failed"); return false; }
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
    const bool ok=cleanTarget();
    clear(ok ? "cancelled" : "cleanup-failed");
    return ok;
}
bool storeManifest(uint32_t id, const char* data, size_t length) {
    if (!touch(id) || manifestStored || !data || !length || length>WebManifest::MaxBytes) return false;
    File f=LittleFS.open(pathFor("web-manifest.json"),"w");
    if (!f) return false;
    bool ok=f.write(reinterpret_cast<const uint8_t*>(data),length)==length;
    f.close();
    f=LittleFS.open(pathFor("web-manifest.json"),"r");
    ok=ok && f && f.size()==length;
    f.close(); manifestStored=ok; return ok;
}
bool chunk(uint32_t id, const char* name, uint32_t offset, const uint8_t* data, size_t length) {
    update();
    if (!id || id!=session.id || !manifestStored || session.verified || !length || length>1024 || !data || session.target==WebSlotManager::activeSlot() || !WebSlotManager::isIncomplete(session.target)) return false;
    int i=WebManifest::assetIndex(name);
    if (i<0 || offset!=offsets[i] || offset>sizes[i] || length>sizes[i]-offset) return false;
    File f=LittleFS.open(pathFor(name),offset?"a":"w");
    if (!f) return false;
    bool ok=f.size()==offset && f.write(data,length)==length;
    f.close();
    if (!ok) { cancel(id); return false; }
    f=LittleFS.open(pathFor(name),"r");
    ok=f && f.size()==offset+length; f.close();
    if (!ok) { cancel(id); return false; }
    offsets[i]+=length; session.received+=length;
    if (offsets[i]==sizes[i]) ++session.completed;
    return touch(id);
}
const char* verify(uint32_t id) {
    if (!touch(id)) return "session";
    if (!manifestStored || session.completed!=session.fileCount || session.received!=session.totalBytes) return "incomplete";
    session.verified=false; session.verifiedFiles=0;
    if (session.target==WebSlotManager::activeSlot() || !WebSlotManager::isIncomplete(session.target)) return "slot";
    // Keep the buffer off the ESP8266's small stack.
    static uint8_t buffer[1024];
    for (size_t i=0;i<WebManifest::AssetCount;++i) {
        File f=LittleFS.open(pathFor(WebManifest::assetName(i)),"r");
        if (!f || f.size()!=sizes[i]) { session.lastError="file-size"; return session.lastError; }
        br_sha256_context ctx;
        br_sha256_init(&ctx);
        size_t remaining=sizes[i];
        while (remaining) {
            size_t n=remaining>sizeof(buffer)?sizeof(buffer):remaining;
            if (f.read(buffer,n)!=n) { session.lastError="file-read"; return session.lastError; }
            br_sha256_update(&ctx,buffer,n); remaining-=n; yield();
        }
        f.close();
        uint8_t digest[32]; br_sha256_out(&ctx,digest);
        if (memcmp(digest,hashes[i],32)) {
            Serial.printf("Web SHA256 mismatch: %s\n",WebManifest::assetName(i));
            session.lastError="sha256-mismatch"; return session.lastError;
        }
        ++session.verifiedFiles;
    }
    session.verified=true; session.lastError=nullptr; session.lastActivity=millis();
    return nullptr;
}

const char* activate(uint32_t id) {
    if (!touch(id) || !session.verified || session.verifiedFiles != WebManifest::AssetCount) return "not-verified";
    if (FirmwareUpdate::active() || Update.isRunning() || WebServer::maintenancePending()) return "operation-active";
    if (const char* error = verify(id)) return error;
    const auto target = session.target;
    const String marker = pathFor(".incomplete");
    if (!LittleFS.remove(marker) || LittleFS.exists(marker)) return "marker-remove";
    // From this point the slot contains a fully verified interface. Never clean it
    // on a selection write failure: a valid record may already be on flash.
    const bool selected = WebSlotManager::persistSelection(target);
    clear(selected ? nullptr : "selection-failed");
    return selected ? nullptr : "selection-failed";
}
const char* rollback() {
    if (active() || FirmwareUpdate::active() || Update.isRunning() || WebServer::maintenancePending()) return "operation-active";
    const auto target = WebSlotManager::inactiveSlot();
    if (!WebSlotManager::hasEntryPoint(target)) return "backup-unavailable";
    for (size_t i=0;i<WebManifest::AssetCount;++i) {
        File f=LittleFS.open(String(WebSlotManager::root(target))+"/"+WebManifest::assetName(i),"r");
        if (!f || f.isDirectory() || !f.size()) return "backup-incomplete";
        f.close();
    }
    return WebSlotManager::persistSelection(target) ? nullptr : "selection-failed";
}

}
