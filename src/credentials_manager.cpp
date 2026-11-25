#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "storage.h"
#include "credentials_manager.h"
#ifdef WORDCLOCK_PRIVATE_MIGRATION
#include "ota_config.h"
#include "web_update_config.h"
#endif

namespace Credentials {
static const char* PATH = "/config/credentials.json";
#ifdef WORDCLOCK_PRIVATE_MIGRATION
static const char* TEMP = "/config/credentials.tmp";
#endif
static char admin[65] = {}, ota[65] = {}, host[64] = {};
static bool loaded = false;
static bool valid(const char* p, size_t minLen, size_t maxLen) {
    return p && strlen(p) >= minLen && strlen(p) <= maxLen && strcmp(p, "CHANGE_ME") != 0;
}
static bool load(const char* path) {
    File f = LittleFS.open(path, "r");
    if (!f || !f.size() || f.size() > 1024) return false;
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, f)) return false;
    const char* a = doc["adminPassword"] | "";
    const char* o = doc["otaPassword"] | "";
    const char* h = doc["otaHostname"] | "";
    if (doc["version"].as<int>() != 1 || !valid(a, 1, 64) || !valid(o, 8, 64) || !valid(h, 1, 63)) return false;
    strcpy(admin, a); strcpy(ota, o); strcpy(host, h);
    return true;
}
void begin() {
    loaded = false;
    if (!Storage::isReady()) return;
    if (LittleFS.exists(PATH)) {
        loaded = load(PATH);
    }
#ifdef WORDCLOCK_PRIVATE_MIGRATION
    else {
        if (!OTAConfig::ENABLED || !WebUpdateConfig::ENABLED || strcmp(WebUpdateConfig::USERNAME, "admin") ||
            !valid(WebUpdateConfig::PASSWORD, 1, 64) || !valid(OTAConfig::PASSWORD, 8, 64) || !valid(OTAConfig::HOSTNAME, 1, 63)) {
            Serial.println("Credentials: invalid migration configuration"); return;
        }
        StaticJsonDocument<512> doc;
        doc["version"] = 1;
        doc["adminPassword"] = WebUpdateConfig::PASSWORD;
        doc["otaPassword"] = OTAConfig::PASSWORD;
        doc["otaHostname"] = OTAConfig::HOSTNAME;
        File f = LittleFS.open(TEMP, "w");
        if (!f) return;
        size_t written = serializeJson(doc, f);
        f.close();
        if (written == measureJson(doc) && load(TEMP) &&
            !strcmp(admin, WebUpdateConfig::PASSWORD) && !strcmp(ota, OTAConfig::PASSWORD) && !strcmp(host, OTAConfig::HOSTNAME) &&
            LittleFS.rename(TEMP, PATH)) loaded = load(PATH);
    }
#endif
    Serial.println(loaded ? "Credentials: persistent configuration ready" : "Credentials: unavailable; maintenance authentication disabled");
}
bool ready() { return loaded; }
const char* adminPassword() { return admin; }
const char* otaPassword() { return ota; }
const char* otaHostname() { return host; }
}
