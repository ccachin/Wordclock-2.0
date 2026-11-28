#include "automation_manager.h"
#include "weather_manager.h"
#include <BearSSLHelpers.h>
#include <new>
extern "C" {
#include <user_interface.h>
}

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <Updater.h>
#include <bearssl/bearssl_hash.h>
#include <bearssl/bearssl_rsa.h>
#include "firmware_signing_key.h"
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

#include "web_server.h"
#include "render_manager.h"
#include "leds.h"
#include "config.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "storage.h"
#include "theme_manager.h"
#include "theme_library.h"
#include "color_manager.h"
#include "credentials_manager.h"
#include "transition_manager.h"
#include "night_manager.h"
#include "custom_theme_store.h"
#include "preview_manager.h"
#include "settings_manager.h"
#include "ota_manager.h"
#include "firmware_update.h"
#include "web_slot_manager.h"
#include "web_manifest.h"
#include "web_update_manager.h"

namespace WebServer
{

    static ESP8266WebServer server(80);

    static bool running = false;
    static bool mdnsRunning = false;
    static bool mdnsAttempted = false;
    static uint32_t mdnsAttemptAt = 0;
    static IPAddress mdnsAddress;
    static constexpr uint32_t MDNS_RETRY_MS = 10000;
    static uint8_t wifiAction = 0;
    static uint32_t wifiActionAt = 0;
    static bool restartPending = false;
    static uint32_t restartAt = 0;
    static bool factoryResetScheduled = false;

    bool maintenancePending() { return restartPending || wifiAction || factoryResetScheduled; }
    // Periodic samples, not a measurement of every transient allocation.
    static uint32_t minHeap = UINT32_MAX, minBlock = UINT32_MAX;
    static void recordMemory(uint32_t heap, uint32_t block) {
        if (heap < minHeap) minHeap = heap;
        if (block < minBlock) minBlock = block;
    }
    static void sampleMemory() {
        static uint32_t previous = 0;
        const uint32_t now = millis();
        if (minHeap != UINT32_MAX && now-previous < 1000) return;
        previous = now;
        recordMemory(ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
    }

    // ==================================================
    // JSON responses
    // ==================================================

    static void sendJson(
        int code,
        const String &json)
    {

        server.sendHeader(
            "Cache-Control",
            "no-store");

        server.send(
            code,
            "application/json",
            json);
    }

    // Read the existing LED buffer in the main loop. No persistent frame copy.
    static void handleDisplayFrame() {
        if (firmwareSessionActive() || FirmwareUpdate::active() || WebUpdateManager::active() || maintenancePending() || OTAManager::isUpdating()) {
            sendJson(409, F("{\"error\":\"Display temporarily unavailable\"}")); return;
        }
        const auto& d=RenderManager::displayState();
        if (!d.ready || ESP.getFreeHeap()<8000 || ESP.getMaxFreeBlockSize()<4000) {
            sendJson(503, F("{\"error\":\"Display temporarily unavailable\"}")); return;
        }
        String json;
        if (!json.reserve(1000)) { sendJson(503, F("{\"error\":\"Insufficient memory\"}")); return; }
        json += F("{\"mode\":"); json += static_cast<unsigned>(d.mode);
        json += F(",\"weather\":"); json += static_cast<unsigned>(d.weather);
        json += F(",\"zone\":"); json += static_cast<unsigned>(d.zone);
        json += F(",\"animated\":"); json += d.animated ? '1' : '0';
        json += F(",\"capped\":"); json += d.capped ? '1' : '0';
        json += F(",\"brightness\":"); json += static_cast<unsigned>(FastLED.getBrightness());
        json += F(",\"temperature\":");
        if (WeatherManager::valid()) json += WeatherManager::temperature(); else json += F("null");
        json += F(",\"pixels\":\"");
        const uint8_t level=FastLED.getBrightness();
        for (uint16_t i=0;i<Config::TOTAL_LED_COUNT;++i) {
            const CRGB c=Leds::getPixel(i);
            const uint8_t channels[]={c.r,c.g,c.b};
            for (uint8_t raw:channels) {
                const uint8_t value=RenderManager::previewChannel(raw,level);
                json += RenderManager::hexNibble(value>>4);
                json += RenderManager::hexNibble(value&15);
            }
        }
        json += F("\"}");
        sendJson(200,json);
    }

    static void sendJsonDocument(int code, const JsonDocument& doc) {
        if (doc.overflowed()) { sendJson(503, "{\"error\":\"Insufficient memory\"}"); return; }
        String json;
        const size_t bytes = measureJson(doc);
        if (!json.reserve(bytes) || serializeJson(doc, json) != bytes || json.length() != bytes) {
            sendJson(503, "{\"error\":\"Insufficient memory\"}");
            return;
        }
        sendJson(code, json);
    }

    // ==================================================
    // Static files
    // ==================================================

    static void appendWebVersion(JsonDocument& doc);

    static void serveWebFile(
        const char *name,
        const char *contentType)
    {

        if (!Storage::isReady())
        {

            server.send(
                503,
                "text/plain",
                "Storage unavailable");

            return;
        }

        if (WebSlotManager::isIncomplete(WebSlotManager::activeSlot())) {
            server.send(503, "text/plain", "Web interface incomplete");
            return;
        }
        const String root = WebSlotManager::activeRoot();
        bool cacheable = false;
        if (strcmp(name, "index.html") && server.hasArg("v") &&
            !LittleFS.exists(root + "/.development")) {
            // Refresh once per slot switch, before opening the response file.
            static uint32_t cachedGeneration = 0;
            static bool cached = false;
            static char cachedVersion[33] = {};
            const uint32_t generation = WebSlotManager::selectionGeneration();
            if (!cached || generation != cachedGeneration) {
                StaticJsonDocument<384> metadata;
                appendWebVersion(metadata);
                const char* version = metadata["webVersion"] | "";
                strlcpy(cachedVersion, version, sizeof(cachedVersion));
                cachedGeneration = generation;
                cached = true;
            }
            cacheable = cachedVersion[0] && server.arg("v") == cachedVersion;
        }
        // Plain originals are always retained for older firmware and clients.
        // Companion files occupy existing, now unused API-1 manifest entries.
        const char* companion = !strcmp(name, "app.js") ? "community.js" :
                                !strcmp(name, "style.css") ? "night.js" : nullptr;
        bool compressed = false;
        File file;
        if (cacheable && companion) {
            const String& encoding = server.header("Accept-Encoding");
            // Ordinary browsers advertise an explicit gzip token. A token with
            // q=0 or an unrecognised quality parameter falls back to plain.
            bool acceptsGzip = false;
            const char* pos = encoding.c_str();
            while (*pos) {
                const char* end = strchr(pos, ',');
                if (!end) end = pos + strlen(pos);
                const char* first = pos;
                const char* last = end;
                while (first < last && isspace(static_cast<unsigned char>(*first))) ++first;
                while (last > first && isspace(static_cast<unsigned char>(last[-1]))) --last;
                if (last-first == 4 && strncasecmp(first, "gzip", 4)==0) { acceptsGzip = true; break; }
                pos = *end ? end+1 : end;
            }
            if (acceptsGzip) {
                file = LittleFS.open(root + "/" + companion, "r");
                if (file && file.size() >= 18 && file.read() == 0x1f &&
                    file.read() == 0x8b && file.read() == 8 && file.seek(0, SeekSet)) {
                    compressed = true;
                } else if (file) { file.close(); }
            }
        }
        if (!compressed) file = LittleFS.open(root + "/" + name, "r");
        if (!file) {
            server.send(404, "text/plain", "File not found");
            return;
        }
        server.sendHeader("Cache-Control", cacheable ? "private, max-age=300" : "no-store");
        if (companion) server.sendHeader("Vary", "Accept-Encoding");
        if (compressed) server.sendHeader("Content-Encoding", "gzip");
        const size_t expected = file.size();
        const size_t sent = server.streamFile(file, contentType);
        file.close();
        if (sent != expected) {
            Serial.printf("Static transfer incomplete: %s sent=%u expected=%u heap=%u maxBlock=%u\n",
                          name, (unsigned)sent, (unsigned)expected,
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
        }
    }

    static void handleRoot()
    {

        serveWebFile(
            "index.html",
            "text/html; charset=utf-8");
    }

    // ==================================================
    // System status
    // ==================================================

    static void handleStatus()
    {

        TimeManager::ClockTime now;

        bool valid = TimeManager::getTime(now);

        char timeString[12] = "";

        if (valid)
        {

            snprintf(
                timeString,
                sizeof(timeString),
                "%02u:%02u:%02u",
                now.hour,
                now.minute,
                now.second);
        }

        char json[256];

        snprintf(
            json,
            sizeof(json),
            "{"
            "\"firmware\":\"" WORDCLOCK_FIRMWARE_VERSION "\","
            "\"connected\":%s,"
            "\"timeValid\":%s,"
            "\"time\":\"%s\","
            "\"ip\":\"%s\","
            "\"rssi\":%ld,"
            "\"uptime\":%lu,"
            "\"freeHeap\":%lu"
            "}",
            WifiManager::isConnected()
                ? "true"
                : "false",
            valid ? "true" : "false",
            timeString,
            WifiManager::getIPAddress().c_str(),
            static_cast<long>(WiFi.RSSI()),
            static_cast<unsigned long>(
                millis() / 1000),
            static_cast<unsigned long>(
                ESP.getFreeHeap()));

        sendJson(200, json);
    }

    // ==================================================
    // Color conversion
    // ==================================================

    static bool parseColor(
        const String &value,
        CRGB &color)
    {

        if (
            value.length() != 7 ||
            value[0] != '#')
        {
            return false;
        }

        for (size_t i = 1; i < 7; i++)
        {

            if (!isxdigit(
                    static_cast<unsigned char>(value[i])))
            {
                return false;
            }
        }

        uint32_t rgb = strtoul(
            value.c_str() + 1,
            nullptr,
            16);

        color = CRGB(
            (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF,
            rgb & 0xFF);

        return true;
    }

    // ==================================================
    // Current theme - GET
    // ==================================================

    static bool validPreviewToken(const String& token) {
        if (token.length() != 32) return false;
        for (size_t i=0;i<token.length();++i) if (!isxdigit(static_cast<unsigned char>(token[i]))) return false;
        return true;
    }
    static bool nightNumber(const char* key, unsigned maxValue, unsigned& result);
    static void handlePreview() {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        const String token = server.arg("token");
        if (!validPreviewToken(token)) { sendJson(400, "{\"error\":\"Invalid token\"}"); return; }
        const String action = server.arg("action");
        if (action == "stop") {
            const bool ok = PreviewManager::stop(token);
            sendJson(ok ? 200 : 409, ok ? "{\"ok\":true}" : "{\"error\":\"Preview busy\"}"); return;
        }
        auto theme = ThemeManager::getTheme();
        if (action != "set" || !parseColor(server.arg("front"), theme.frontColor) ||
            !parseColor(server.arg("rear"), theme.rearColor) ||
            !ColorManager::harmonyFromString(server.arg("harmony"), theme.harmony)) {
            sendJson(400, "{\"error\":\"Invalid preview\"}"); return;
        }
        for (int face=0;face<2;++face) {
            auto& a = face == 0 ? theme.frontAnimation : theme.rearAnimation;
            const String speed = server.arg(face == 0 ? "frontSpeed" : "rearSpeed");
            bool valid = speed.length() > 0 && speed.length() <= 3;
            for (size_t i=0;i<speed.length();++i) if (speed[i]<'0'||speed[i]>'9') valid=false;
            if (!valid || speed.toInt()>100 || !AnimationManager::effectFromString(server.arg(face == 0 ? "frontEffect" : "rearEffect"), a.effect) ||
                (face==0 && (a.effect==AnimationManager::Effect::Rainbow || a.effect==AnimationManager::Effect::Chase))) {
                sendJson(400, "{\"error\":\"Invalid animation\"}"); return;
            }
            a.speed = speed.toInt();
        }
        unsigned level;
        if (server.hasArg("frontBrightness")) {
            if (!nightNumber("frontBrightness", 100, level)) { sendJson(400, "{\"error\":\"Invalid frontBrightness\"}"); return; }
            theme.frontBrightness = level;
        }
        if (server.hasArg("rearBrightness")) {
            if (!nightNumber("rearBrightness", 100, level)) { sendJson(400, "{\"error\":\"Invalid rearBrightness\"}"); return; }
            theme.rearBrightness = level;
        }
        const bool ok = PreviewManager::set(token, theme);
        sendJson(ok ? 200 : 409, ok ? "{\"ok\":true}" : "{\"error\":\"Preview busy or offline\"}");
    }

    static void handleGetNight() {
        const auto& n = NightManager::getSettings();
        TimeManager::ClockTime now{};
        const bool valid = TimeManager::getTime(now);
        char json[192];
        snprintf(json, sizeof(json), "{\"enabled\":%s,\"startMinute\":%u,\"endMinute\":%u,\"brightness\":%u,\"mode\":%u,\"active\":%s}",
            n.enabled ? "true" : "false", unsigned(n.startMinute), unsigned(n.endMinute), unsigned(n.brightness),
            unsigned(n.mode), NightManager::isActive(valid, now.hour, now.minute) ? "true" : "false");
        sendJson(200, json);
    }
    static bool nightNumber(const char* key, unsigned maxValue, unsigned& result) {
        const String value = server.arg(key);
        if (value.length() == 0 || value.length() > 4) return false;
        for (unsigned i = 0; i < value.length(); ++i)
            if (value[i] < '0' || value[i] > '9') return false;
        result = value.toInt();
        return result <= maxValue;
    }
    static void handleSetNight() {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        auto n = NightManager::getSettings();
        unsigned value;
        if (server.hasArg("enabled")) {
            const String enabled = server.arg("enabled");
            if (enabled != "0" && enabled != "1") { sendJson(400, "{\"error\":\"Invalid enabled\"}"); return; }
            n.enabled = enabled == "1";
        }
        if (server.hasArg("startMinute")) {
            if (!nightNumber("startMinute", 1439, value)) { sendJson(400, "{\"error\":\"Invalid startMinute\"}"); return; }
            n.startMinute = value;
        }
        if (server.hasArg("endMinute")) {
            if (!nightNumber("endMinute", 1439, value)) { sendJson(400, "{\"error\":\"Invalid endMinute\"}"); return; }
            n.endMinute = value;
        }
        if (server.hasArg("brightness")) {
            if (!nightNumber("brightness", 255, value) || value == 0) { sendJson(400, "{\"error\":\"Invalid brightness\"}"); return; }
            n.brightness = value;
        }
        if (server.hasArg("mode")) {
            if (!nightNumber("mode", 2, value)) { sendJson(400, "{\"error\":\"Invalid mode\"}"); return; }
            n.mode = static_cast<NightManager::Mode>(value);
        }
        if (!NightManager::setSettings(n)) { sendJson(400, "{\"error\":\"Invalid night settings\"}"); return; }
        handleGetNight();
    }

    static void handleGetTransitions() {
        char json[64];
        snprintf(json, sizeof(json), "{\"duration\":%u}", static_cast<unsigned>(TransitionManager::getDuration()));
        sendJson(200, json);
    }
    static void handleSetTransitions() {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        String value = server.arg("duration");
        if (value.length() == 0 || value.length() > 4) { sendJson(400, "{\"error\":\"Invalid duration\"}"); return; }
        for (unsigned i = 0; i < value.length(); ++i)
            if (value[i] < '0' || value[i] > '9') { sendJson(400, "{\"error\":\"Invalid duration\"}"); return; }
        long duration = value.toInt();
        if (duration > TransitionManager::MAX_DURATION) { sendJson(400, "{\"error\":\"Invalid duration\"}"); return; }
        TransitionManager::setDuration(static_cast<uint16_t>(duration));
        handleGetTransitions();
    }

    static void handleGetAnimations() {
        const auto& a = AnimationManager::frontSettings();
        char json[200];
        snprintf(json,sizeof(json),"{\"enabled\":%s,\"hourlyColor\":%s,\"transition\":%u,\"chime\":%u,\"spatial\":%u,\"spatialSpeed\":%u,\"intensity\":%u,\"duration\":%u}",
            a.enabled?"true":"false", a.hourlyColor?"true":"false", unsigned(a.transition), unsigned(a.chime),
            unsigned(a.spatial), unsigned(a.spatialSpeed), unsigned(a.intensity), unsigned(a.duration));
        sendJson(200,json);
    }
    static bool animationActionAllowed() {
        if (firmwareSessionActive() || WebUpdateManager::active() || maintenancePending() || FirmwareUpdate::active()) {
            sendJson(409,"{\"error\":\"Maintenance active\"}"); return false;
        }
        if (server.header("X-Wordclock-Action") != "animations") {
            sendJson(403,"{\"error\":\"Missing action header\"}"); return false;
        }
        return true;
    }
    static void handleSetAnimations() {
        if (!animationActionAllowed()) return;
        auto a=AnimationManager::frontSettings();
        unsigned enabled,hourly,transition,chime,spatial,spatialSpeed,intensity,duration;
        if (!nightNumber("enabled",1,enabled) || !nightNumber("hourlyColor",1,hourly) ||
            !nightNumber("transition",FrontEffects::Circle,transition) || !nightNumber("chime",2,chime) ||
            !nightNumber("spatial",2,spatial) || !nightNumber("spatialSpeed",100,spatialSpeed) || !nightNumber("intensity",100,intensity) || !nightNumber("duration",5000,duration)) {
            sendJson(400,"{\"error\":\"Invalid animation settings\"}"); return;
        }
        a.enabled=enabled; a.hourlyColor=hourly; a.transition=transition; a.chime=chime;
        a.spatial=spatial; a.spatialSpeed=spatialSpeed; a.intensity=intensity; a.duration=duration;
        if (!AnimationManager::setFrontSettings(a)) { sendJson(400,"{\"error\":\"Invalid animation settings\"}"); return; }
        handleGetAnimations();
    }
    static void handleTestAnimation() {
        if (!animationActionAllowed()) return;
        unsigned kind;
        if (!nightNumber("kind",FrontEffects::Circle,kind) || !kind) { sendJson(400,"{\"error\":\"Invalid animation\"}"); return; }
        TimeManager::ClockTime now{};
        const auto& theme=ThemeManager::getTheme();
        if (!TimeManager::getTime(now) || NightManager::isActive(true,now.hour,now.minute) ||
            PreviewManager::active() || !theme.brightness || !theme.frontBrightness ||
            !AnimationManager::startFrontEffect(static_cast<FrontEffects::Kind>(kind),millis())) {
            sendJson(409,"{\"error\":\"Animation unavailable (night, clock off, preview or disabled)\"}"); return;
        }
        sendJson(200,"{\"ok\":true}");
    }

    static void handleGetWeather(){
        const auto& c=WeatherManager::config(); char json[320];
        snprintf(json,sizeof(json),"{\"lat\":%ld,\"lon\":%ld,\"enabled\":%u,\"forecast\":%u,\"zone\":%u,\"intensity\":%u,\"valid\":%s,\"code\":%d,\"day\":%d,\"temperature\":%d,\"age\":%lu,\"status\":\"%s\"}",(long)c.lat,(long)c.lon,c.enabled,c.forecast,c.zone,c.intensity,WeatherManager::valid()?"true":"false",WeatherManager::code(),WeatherManager::day(),WeatherManager::temperature(),(unsigned long)WeatherManager::age(),WeatherManager::status());sendJson(200,json);
    }
    static bool weatherNumber(const char* key,long min,long max,long& v){const String a=server.arg(key);if(!a.length()||a.length()>12)return false;char* end=nullptr;v=strtol(a.c_str(),&end,10);return end&&*end==0&&v>=min&&v<=max;}
    static void handleSetWeather(){
        if(firmwareSessionActive()||WebUpdateManager::active()||maintenancePending()){sendJson(409,"{\"error\":\"Maintenance active\"}");return;}
        if(server.header("X-Wordclock-Action")!="weather"){sendJson(403,"{\"error\":\"Missing action header\"}");return;}
        if(server.arg("refresh")=="1"){WeatherManager::refresh();handleGetWeather();return;}
        long lat,lon,en,fc,z,in;if(!weatherNumber("lat",-9000000,9000000,lat)||!weatherNumber("lon",-18000000,18000000,lon)||!weatherNumber("enabled",0,1,en)||!weatherNumber("forecast",0,1,fc)||!weatherNumber("zone",0,2,z)||!weatherNumber("intensity",0,100,in)){sendJson(400,"{\"error\":\"Invalid weather settings\"}");return;}
        WeatherManager::Config c;c.lat=lat;c.lon=lon;c.enabled=en;c.forecast=fc;c.zone=z;c.intensity=in;if(!WeatherManager::save(c)){sendJson(503,"{\"error\":\"Storage failure\"}");return;}handleGetWeather();
    }
    static void handleGetRules() {
        char hex[AutomationRules::HexLength+1]; AutomationManager::encode(hex);
        char json[AutomationRules::HexLength+40];
        snprintf(json,sizeof(json),"{\"version\":1,\"rules\":\"%s\"}",hex); sendJson(200,json);
    }
    static void handleSetRules() {
        if (firmwareSessionActive() || WebUpdateManager::active() || maintenancePending() || FirmwareUpdate::active()) { sendJson(409,"{\"error\":\"Maintenance active\"}"); return; }
        if (server.header("X-Wordclock-Action") != "automations") { sendJson(403,"{\"error\":\"Missing action header\"}"); return; }
        const String& hex=server.arg("rules");
        if (!AutomationManager::replace(hex.c_str(),hex.length())) { sendJson(400,"{\"error\":\"Invalid rules or storage failure\"}"); return; }
        handleGetRules();
    }

    static void handleGetDevice() {
        const auto value = SettingsManager::deviceSettings();
        StaticJsonDocument<512> doc;
        doc["name"] = value.name;
        doc["timezone"] = value.timezone;
        doc["setupComplete"] = value.setupComplete;
        doc["savePending"] = SettingsManager::savePending();
        doc["saveFailed"] = SettingsManager::saveFailed();
        sendJsonDocument(200, doc);
    }
    static void handleSetDevice() {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        auto value = SettingsManager::deviceSettings();
        if (!server.hasArg("name") && !server.hasArg("timezone") && !server.hasArg("setupComplete")) { sendJson(400, "{\"error\":\"Missing device settings\"}"); return; }
        if (server.hasArg("name")) { value.name = server.arg("name"); value.name.trim(); }
        if (server.hasArg("timezone")) value.timezone = server.arg("timezone");
        if (server.hasArg("setupComplete")) {
            const String flag = server.arg("setupComplete");
            if (flag != "true" && flag != "false") { sendJson(400, "{\"error\":\"Invalid setupComplete\"}"); return; }
            value.setupComplete = flag == "true";
        }
        if (!SettingsManager::validDeviceSettings(value)) { sendJson(400, "{\"error\":\"Invalid device settings\"}"); return; }
        if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
        if (!SettingsManager::setDeviceSettings(value)) { sendJson(400, "{\"error\":\"Invalid device settings\"}"); return; }
        Serial.printf("Device settings accepted; pending=%d, heap=%u\n", SettingsManager::savePending(), static_cast<unsigned>(ESP.getFreeHeap()));
        handleGetDevice();
    }

    static void handleGetAppearance() {
        char json[40];
        snprintf(json, sizeof(json), "{\"faceColor\":\"#%06lX\"}", static_cast<unsigned long>(SettingsManager::faceColor()));
        sendJson(200, json);
    }
    static void handleSetAppearance() {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        CRGB color;
        if (!parseColor(server.arg("faceColor"), color)) { sendJson(400, "{\"error\":\"Invalid face color\"}"); return; }
        SettingsManager::setFaceColor((uint32_t(color.r)<<16) | (uint32_t(color.g)<<8) | color.b);
        handleGetAppearance();
    }

    static void handleGetTheme()
    {

        const auto &theme =
            ThemeManager::getTheme();

        CRGB effective =
            ThemeManager::getEffectiveRearColor();

        char json[384];

        snprintf(
            json,
            sizeof(json),
            "{"
            "\"front\":\"#%02X%02X%02X\","
            "\"rear\":\"#%02X%02X%02X\","
            "\"effectiveRear\":\"#%02X%02X%02X\","
            "\"brightness\":%u,\"frontBrightness\":%u,\"rearBrightness\":%u,"
            "\"harmony\":\"%s\",\"frontEffect\":\"%s\",\"rearEffect\":\"%s\",\"frontSpeed\":%u,\"rearSpeed\":%u"
            "}",
            theme.frontColor.r,
            theme.frontColor.g,
            theme.frontColor.b,
            theme.rearColor.r,
            theme.rearColor.g,
            theme.rearColor.b,
            effective.r,
            effective.g,
            effective.b,
            theme.brightness,
            unsigned(theme.frontBrightness), unsigned(theme.rearBrightness),
            ColorManager::harmonyToString(
                theme.harmony),
            AnimationManager::effectToString(theme.frontAnimation.effect),
            AnimationManager::effectToString(theme.rearAnimation.effect),
            static_cast<unsigned>(theme.frontAnimation.speed),
            static_cast<unsigned>(theme.rearAnimation.speed));

        sendJson(200, json);
    }

    // ==================================================
    // Current theme - POST
    // ==================================================

    static void handleSetTheme()
    {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }

        ThemeManager::Theme theme =
            ThemeManager::getTheme();

        if (server.hasArg("front"))
        {

            if (!parseColor(
                    server.arg("front"),
                    theme.frontColor))
            {

                sendJson(
                    400,
                    "{\"error\":\"Invalid front color\"}");

                return;
            }
        }

        if (server.hasArg("rear"))
        {

            if (!parseColor(
                    server.arg("rear"),
                    theme.rearColor))
            {

                sendJson(
                    400,
                    "{\"error\":\"Invalid rear color\"}");

                return;
            }
        }

        if (server.hasArg("brightness"))
        {

            String value =
                server.arg("brightness");

            char *end = nullptr;

            long brightness = strtol(
                value.c_str(),
                &end,
                10);

            if (
                value.length() == 0 ||
                end == value.c_str() ||
                *end != '\0' ||
                brightness < 0 ||
                brightness > 255)
            {

                sendJson(
                    400,
                    "{\"error\":\"Invalid brightness\"}");

                return;
            }

            theme.brightness =
                static_cast<uint8_t>(brightness);
        }

        unsigned faceBrightness;
        if (server.hasArg("frontBrightness")) {
            if (!nightNumber("frontBrightness", 100, faceBrightness)) { sendJson(400, "{\"error\":\"Invalid frontBrightness\"}"); return; }
            theme.frontBrightness = faceBrightness;
        }
        if (server.hasArg("rearBrightness")) {
            if (!nightNumber("rearBrightness", 100, faceBrightness)) { sendJson(400, "{\"error\":\"Invalid rearBrightness\"}"); return; }
            theme.rearBrightness = faceBrightness;
        }
        if (server.hasArg("harmony"))
        {

            if (!ColorManager::harmonyFromString(
                    server.arg("harmony"),
                    theme.harmony))
            {

                sendJson(
                    400,
                    "{\"error\":\"Invalid harmony\"}");

                return;
            }
        }

        for (int face = 0; face < 2; ++face) {
            bool front = face == 0;
            auto &animation = front ? theme.frontAnimation : theme.rearAnimation;
            const char* effectKey = front ? "frontEffect" : "rearEffect";
            const char* speedKey = front ? "frontSpeed" : "rearSpeed";
            if (server.hasArg(effectKey) &&
                (!AnimationManager::effectFromString(server.arg(effectKey), animation.effect) ||
                 (front && (animation.effect == AnimationManager::Effect::Rainbow || animation.effect == AnimationManager::Effect::Chase)))) {
                sendJson(400, "{\"error\":\"Invalid effect\"}"); return;
            }
            if (server.hasArg(speedKey)) {
                String value = server.arg(speedKey);
                char* end = nullptr;
                long speed = strtol(value.c_str(), &end, 10);
                if (!value.length() || end == value.c_str() || *end || speed < 0 || speed > 100) {
                    sendJson(400, "{\"error\":\"Invalid speed\"}"); return;
                }
                animation.speed = static_cast<uint8_t>(speed);
            }
        }
        ThemeManager::setTheme(theme);

        handleGetTheme();
    }

    // ==================================================
    // Theme library
    // ==================================================

    // Keep the validated v2 web client working until the Studio migration.
    static uint8_t responseThemeVersion() { return server.arg("version") == "3" ? 3 : 2; }

    static void handleGetThemes()
    {
        const uint8_t version = responseThemeVersion();
        const bool factoryOnly = server.arg("factoryOnly") == "1";
        String ids;
        DynamicJsonDocument list(factoryOnly ? 0 : JSON_ARRAY_SIZE(CustomThemeStore::MAX_THEMES));
        if (!factoryOnly && Storage::isReady()) {
            if (!CustomThemeStore::listIds(ids) ||
                deserializeJson(list, const_cast<char*>(ids.c_str()), ids.length()) || !list.is<JsonArray>()) {
                sendJson(503, "{\"error\":\"Custom catalog unavailable\"}");
                return;
            }
        }
        server.sendHeader("Cache-Control", "no-store");
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "application/json", "");
        server.sendContent("[");
        bool comma = false;
        for (size_t i=0;i<ThemeLibrary::factoryCount();++i) {
            if (!server.client().connected()) return;
            char factoryJson[ThemeLibrary::FactoryEntryCapacity];
            if (!ThemeLibrary::factoryEntry(i, factoryJson, sizeof(factoryJson))) { server.client().stop(); return; }
            const char* entry = factoryJson;
            if (comma) server.sendContent(",");
            if (version==3) {
                // All built-in entries start with this fixed v2 prefix.
                constexpr char prefix[] = "{\"version\":2,";
                if (strncmp(entry,prefix,sizeof(prefix)-1)!=0) { server.client().stop(); return; }
                server.sendContent("{\"version\":3,\"frontBrightness\":100,\"rearBrightness\":100,");
                entry += sizeof(prefix)-1;
            }
            server.sendContent(entry); comma=true;
        }
        for (JsonVariantConst item : list.as<JsonArrayConst>()) {
            if (!server.client().connected()) return;
            ThemeCodec::Entry entry;
            String encoded;
            if (!CustomThemeStore::load(item.as<const char*>(), entry) || !ThemeCodec::encode(entry, encoded, version)) {
                // An incomplete response is preferable to silently reporting a missing theme.
                Serial.printf("Custom catalog: entry unavailable heap=%u max=%u\n", ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
                server.client().stop(); return;
            }
            if (comma) server.sendContent(",");
            server.sendContent(encoded); comma=true;
            yield();
        }
        server.sendContent("]");
        server.sendContent("");
    }

    static void handleApplyTheme()
    {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }

        if (!server.hasArg("id"))
        {

            sendJson(
                400,
                "{\"error\":\"Missing theme id\"}");

            return;
        }

        String id = server.arg("id");

        if (id.length() == 0 || id.length() > 64)
        {

            sendJson(
                400,
                "{\"error\":\"Invalid theme id\"}");

            return;
        }

        if (!ThemeLibrary::applyTheme(id))
        {

            sendJson(
                404,
                "{\"error\":\"Theme not found or invalid\"}");

            return;
        }

        handleGetTheme();
    }

    // ==================================================
    // Web update - Authentication
    // ==================================================

    static bool authorizeWebUpdate()
    {

        if (
            !Credentials::ready() ||
            strcmp(
                Credentials::adminPassword(),
                "CHANGE_ME") == 0)
        {

            sendJson(
                403,
                "{\"error\":\"Web update disabled\"}");

            return false;
        }

        if (!server.authenticate(
                "admin",
                Credentials::adminPassword()))
        {

            server.requestAuthentication();

            return false;
        }

        return true;
    }

    // Multipart file receiver: allocate at most MaxBytes, only after auth.
    static struct {
        char* data = nullptr;
        size_t length = 0;
        size_t capacity = 0;
        int error = 0;
        bool seen = false;
        bool ended = false;
    } manifestRx;

    static void clearManifestRx() {
        free(manifestRx.data);
        manifestRx = {};
    }

    static void handleManifestChunk() {
        if (firmwareSessionActive()) return;
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            if (manifestRx.seen) { manifestRx.error = 400; return; }
            manifestRx.seen = true;
            if (server.header("X-Wordclock-Action") != "web-update" ||
                !Credentials::ready() || strcmp(Credentials::adminPassword(), "CHANGE_ME") == 0) {
                manifestRx.error = 403; return;
            }
            if (!server.authenticate("admin", Credentials::adminPassword())) {
                manifestRx.error = 401; return;
            }
            if (upload.name != "manifest") { manifestRx.error = 400; return; }

        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (manifestRx.error || !manifestRx.seen) return;
            if (upload.currentSize > WebManifest::MaxBytes - manifestRx.length) {
                manifestRx.error = 413;
                free(manifestRx.data); manifestRx.data = nullptr;
                return;
            }
            if (!upload.currentSize) return;
            const size_t needed = manifestRx.length + upload.currentSize;
            if (needed > manifestRx.capacity) {
                const size_t capacity = ((needed + 511) / 512) * 512;
                char* next = static_cast<char*>(realloc(manifestRx.data, capacity));
                if (!next) {
                    Serial.printf("Manifest receive allocation failed: need=%u heap=%u maxBlock=%u\n", static_cast<unsigned>(capacity), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
                    manifestRx.error = 503;
                    free(manifestRx.data); manifestRx.data = nullptr;
                    return;
                }
                manifestRx.data = next;
                manifestRx.capacity = capacity;
            }
            memcpy(manifestRx.data + manifestRx.length, upload.buf, upload.currentSize);
            manifestRx.length += upload.currentSize;
        } else if (upload.status == UPLOAD_FILE_END) {
            manifestRx.ended = true;
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            clearManifestRx();
        }
    }

    static void handleWebUpdatePreflight() {
        if (firmwareSessionActive()) { clearManifestRx(); sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        if (server.header("X-Wordclock-Action") != "web-update") {
            clearManifestRx(); sendJson(403, "{\"error\":\"Action required\"}"); return;
        }
        if (!authorizeWebUpdate()) { clearManifestRx(); return; }
        if (manifestRx.error || !manifestRx.ended || !manifestRx.data) {
            const int status = manifestRx.error ? manifestRx.error : 400;
            clearManifestRx();
            sendJson(status, "{\"valid\":false,\"error\":\"manifest-receive\"}"); return;
        }
        WebManifest::Result result{};
        Serial.printf("Manifest before parse: bytes=%u heap=%u maxBlock=%u\n", static_cast<unsigned>(manifestRx.length), ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
        const bool starting = server.uri() == "/api/system/web-update/start";
        const bool valid = WebManifest::validate(manifestRx.data, manifestRx.length, result);
        if (!starting || !valid) clearManifestRx();
        if (!valid) {
            Serial.printf("Manifest validation failed: %s heap=%u maxBlock=%u\n", result.error, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
            sendJson(strcmp(result.error, "memory") == 0 ? 503 : 400,
                String("{\"valid\":false,\"error\":\"") + result.error + "\"}"); return;
        }
        const auto check = WebUpdateManager::preflight(result);
        int status = 200;
        if (check.error) {
            if (strcmp(check.error, "insufficient-space") == 0) status = 507;
            else if (strcmp(check.error, "invalid-manifest") == 0 ||
                     strcmp(check.error, "payload-size") == 0) status = 400;
            else if (strcmp(check.error, "operation-active") == 0) status = 409;
            else status = 503;
        }
        if (starting) {
            uint32_t id=0;
            if (check.error) { clearManifestRx(); sendJson(status, String("{\"error\":\"")+check.error+"\"}"); return; }
            if (!WebUpdateManager::start(result,id) || !WebUpdateManager::storeManifest(id,manifestRx.data,manifestRx.length)) {
                if (id) WebUpdateManager::cancel(id);
                clearManifestRx(); sendJson(503,"{\"error\":\"start-failed\"}"); return;
            }
            clearManifestRx();
            sendJson(200,String("{\"id\":")+String(id)+",\"target\":\""+(WebSlotManager::inactiveSlot()==WebSlotManager::Slot::A?"A":"B")+"\"}"); return;
        }
        StaticJsonDocument<512> doc;
        doc["valid"] = true;
        doc["ready"] = !check.error;
        if (check.error) doc["error"] = check.error;
        doc["version"] = result.version;
        doc["fileCount"] = result.fileCount;
        doc["totalBytes"] = result.totalBytes;
        doc["source"] = WebSlotManager::activeSlot() == WebSlotManager::Slot::A ? "A" : "B";
        doc["target"] = WebSlotManager::inactiveSlot() == WebSlotManager::Slot::A ? "A" : "B";
        if (check.space.requiredBytes) {
            doc["freeBytes"] = check.space.freeBytes;
            doc["requiredBytes"] = check.space.requiredBytes;
            doc["marginBytes"] = static_cast<int32_t>(check.space.freeBytes) -
                                 static_cast<int32_t>(check.space.requiredBytes);
        }
        if (doc.overflowed()) {
            sendJson(503, "{\"ready\":false,\"error\":\"memory\"}"); return;
        }
        sendJsonDocument(status, doc);
    }

    static void handleClockInfo() {
        char local[24] = "";
        TimeManager::localDateTime(local, sizeof(local));
        char json[128];
        snprintf(json, sizeof(json), "{\"valid\":%s,\"local\":\"%s\",\"source\":\"%s\"}",
                 TimeManager::isValid() ? "true" : "false", local, TimeManager::source());
        sendJson(200, json);
    }

    static bool clockDecimal(const String& value, uint64_t& number) {
        if (!value.length() || value.length() > 10) return false;
        number = 0;
        for (size_t i = 0; i < value.length(); ++i) {
            char c = value[i];
            if (c < '0' || c > '9') return false;
            number = number * 10 + (c - '0');
        }
        return true;
    }

    static void handleClockSet() {
        if (firmwareSessionActive() || WebUpdateManager::active() || restartPending || wifiAction) {
            sendJson(409, "{\"error\":\"Maintenance active\"}"); return;
        }
        if (server.header("X-Wordclock-Action") != "clock") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return;
        }
        bool ok = false;
        if (server.hasArg("epoch")) {
            uint64_t epoch;
            ok = clockDecimal(server.arg("epoch"), epoch) && TimeManager::setEpoch(epoch);
        } else {
            const char* fields[] = {"year", "month", "day", "hour", "minute", "second"};
            uint64_t values[6]{};
            bool valid = true;
            for (size_t i = 0; i < 6; ++i)
                valid = valid && clockDecimal(server.arg(fields[i]), values[i]);
            if (valid && values[0] >= 2020 && values[0] <= 2099 && values[1] <= 12 && values[2] <= 31 && values[3] <= 23 && values[4] <= 59 && values[5] <= 59)
                ok = TimeManager::setTime(values[0], values[1], values[2], values[3], values[4], values[5]);
        }
        if (!ok) { sendJson(400, "{\"error\":\"Invalid date or time\"}"); return; }
        handleClockInfo();
    }

    static void handleWifiInfo()
    {
        DynamicJsonDocument doc(512);
        doc["ssid"] = WifiManager::isConnected() ? WiFi.SSID() : (WifiManager::isOfflineAP() ? WiFi.softAPSSID() : String());
        doc["offlineAP"] = WifiManager::isOfflineAP();
        doc["connected"] = WifiManager::isConnected();
        doc["ip"] = WifiManager::getIPAddress();
        doc["hostname"] = "wordclock.local";
        sendJsonDocument(200, doc);
    }

    static void appendWebVersion(JsonDocument& doc)
    {
        doc["webVersion"] = nullptr;
        doc["webVersionState"] = "unknown";
        if (!Storage::isReady()) return;
        const String root = WebSlotManager::activeRoot();
        if (LittleFS.exists(root + "/.development")) {
            doc["webVersionState"] = "development"; return;
        }
        File f = LittleFS.open(root + "/web-manifest.json", "r");
        if (!f) { doc["webVersionState"] = "legacy"; return; }
        if (!f.size() || f.size() > WebManifest::MaxBytes) {
            doc["webVersionState"] = "invalid"; return;
        }
        // Read only the version metadata, not all asset hashes, on diagnostics.
        StaticJsonDocument<128> filter;
        filter["format"] = true;
        filter["manifestVersion"] = true;
        filter["version"] = true;
        filter["compatibility"]["webApiVersion"] = true;
        StaticJsonDocument<256> meta;
        const auto error = deserializeJson(meta, f, DeserializationOption::Filter(filter));
        f.close();
        const char* version = meta["version"] | "";
        if (error || strcmp(meta["format"] | "", "wordclock-web") ||
            meta["manifestVersion"].as<int>() != 1 ||
            meta["compatibility"]["webApiVersion"].as<int>() != WORDCLOCK_WEB_API_VERSION ||
            !*version || strlen(version) > 32) {
            doc["webVersionState"] = "invalid"; return;
        }
        doc["webVersion"] = String(version);
        doc["webVersionState"] = "manifest";
    }

    static void handleSystemInfo()
    {
        DynamicJsonDocument doc(1152);
        doc["firmware"] = WORDCLOCK_FIRMWARE_VERSION;
        doc["buildId"] = __DATE__ " " __TIME__;
        doc["hostname"] = "wordclock.local";
        doc["ip"] = WifiManager::getIPAddress();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime"] = millis() / 1000;
        const uint32_t heap = ESP.getFreeHeap(), block = ESP.getMaxFreeBlockSize();
        recordMemory(heap, block);
        doc["freeHeap"] = heap;
        doc["maxFreeBlock"] = block;
        doc["heapFragmentation"] = ESP.getHeapFragmentation();
        doc["sampledMinHeap"] = minHeap;
        doc["sampledMinBlock"] = minBlock;
        // Indicative snapshot only: the authorization/transfer guards remain authoritative.
        doc["updateMemoryMargin"] = heap >= 7500 && block >= 6500;
        doc["weatherStatus"] = WeatherManager::status();
        doc["weatherValid"] = WeatherManager::valid();
        doc["flashBytes"] = ESP.getFlashChipRealSize();
        doc["flashConfiguredBytes"] = ESP.getFlashChipSize();
        doc["resetReason"] = ESP.getResetReason();
        doc["sketchBytes"] = ESP.getSketchSize();
        doc["freeSketchBytes"] = ESP.getFreeSketchSpace();
        doc["storageReady"] = Storage::isReady();
        FSInfo info;
        if (Storage::isReady() && LittleFS.info(info)) {
            doc["storageTotal"] = info.totalBytes;
            doc["storageUsed"] = info.usedBytes;
        }
        doc["savePending"] = SettingsManager::savePending();
        doc["saveFailed"] = SettingsManager::saveFailed();
        doc["restartPending"] = restartPending;
        doc["webSlot"] = WebSlotManager::activeSlot() == WebSlotManager::Slot::A ? "A" : "B";
        doc["webSlotSource"] = WebSlotManager::selectionSource();
        doc["webSlotGeneration"] = WebSlotManager::selectionGeneration();
        doc["webApiVersion"] = WORDCLOCK_WEB_API_VERSION;
        doc["releaseChannel"] = WORDCLOCK_RELEASE_CHANNEL;
        appendWebVersion(doc);
        if (doc.overflowed()) { sendJson(503, "{\"error\":\"Insufficient memory\"}"); return; }
        sendJsonDocument(200, doc);
    }

    static void handleRestart()
    {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        if (server.header("X-Wordclock-Action") != "restart") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return;
        }
        if (!authorizeWebUpdate()) return;
        if (WebUpdateManager::active() || restartPending || wifiAction || SettingsManager::savePending() || SettingsManager::saveFailed()) {
            sendJson(409, "{\"error\":\"Action or settings save pending\"}"); return;
        }
        if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
        restartPending = true;
        restartAt = millis();
        sendJson(202, "{\"ok\":true,\"restartPending\":true}");
    }

    static void handleResetSettings()
    {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        if (server.header("X-Wordclock-Action") != "reset-settings") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return;
        }
        if (!authorizeWebUpdate()) return;
        if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
        if (restartPending || wifiAction || PreviewManager::active() || SettingsManager::savePending() || SettingsManager::saveFailed()) {
            sendJson(409, "{\"error\":\"Action, preview or settings save pending\"}"); return;
        }
        TransitionManager::cancel();
        if (!AutomationManager::reset()) { sendJson(503,"{\"error\":\"Cannot reset automations\"}"); return; }
        if (!WeatherManager::reset()) { sendJson(503,"{\"error\":\"Cannot reset weather\"}"); return; }
        ThemeManager::begin();
        NightManager::setSettings(NightManager::Settings());
        TransitionManager::setDuration(TransitionManager::DEFAULT_DURATION);
        AnimationManager::setFrontSettings(FrontEffects::Settings());
        AnimationManager::cancelFrontEffect();
        SettingsManager::markChanged();
        Serial.println("Maintenance: settings reset accepted; save pending");
        sendJson(202, "{\"ok\":true,\"savePending\":true,\"saveFailed\":false}");
    }

    static void handleFactoryReset()
    {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        if (server.header("X-Wordclock-Action") != "factory-reset") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return;
        }
        if (!authorizeWebUpdate()) return;
        if (WebUpdateManager::active()) { sendJson(409, "{\"error\":\"Web update active\"}"); return; }
        if (server.arg("confirm") != "ERASE") {
            sendJson(400, "{\"error\":\"Explicit confirmation required\"}"); return;
        }
        if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
        if (restartPending || wifiAction || PreviewManager::active() || SettingsManager::savePending() || SettingsManager::saveFailed()) {
            sendJson(409, "{\"error\":\"Action, preview or settings save pending\"}"); return;
        }
        if (!Storage::requestFactoryReset()) {
            sendJson(503, "{\"error\":\"Cannot record reset request\"}"); return;
        }
        factoryResetScheduled = true;
        restartPending = true;
        restartAt = millis();
        sendJson(202, "{\"ok\":true,\"factoryResetPending\":true}");
    }

    static void handleWifiAction()
    {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        // Custom header prevents cross-origin HTML form submissions.
        if (server.header("X-Wordclock-Action") != "wifi") {
            sendJson(403, "{\"error\":\"Missing action header\"}");
            return;
        }
        if (!(WifiManager::isOfflineAP() && !WifiManager::isConnected() && !Credentials::ready() && server.arg("action") == "change") && !authorizeWebUpdate()) return;
        if (WebUpdateManager::active() || wifiAction || restartPending) { sendJson(409, "{\"error\":\"Action pending\"}"); return; }
        const String action = server.arg("action");
        if (action != "change" && action != "forget") {
            sendJson(400, "{\"error\":\"Invalid action\"}"); return;
        }
        if (!SettingsManager::save()) {
            sendJson(503, "{\"error\":\"Settings save failed\"}"); return;
        }
        wifiAction = action == "forget" ? 2 : 1;
        wifiActionAt = millis();
        sendJson(200, "{\"ok\":true}");
    }

    // ==================================================
    // Web update - Allowed files
    // ==================================================

    static const char* const WEB_ASSETS[] = {
        "index.html", "style.css", "app.js", "night.js", "studio.js", "theme-rules.js",
        "studio-visual.js", "community.js", "setup.js", "maintenance.js", "icon.svg", "manifest.json"
    };

    static bool isAllowedWebFile(const String &name)
    {
        for (const char* asset : WEB_ASSETS) if (name == asset) return true;
        return false;
    }

    // Called only during boot, before accepting any upload requests.
    static void cleanupAbandonedWebUploads()
    {
        if (!Storage::isReady()) return;
        for (const char* asset : WEB_ASSETS) {
            const String path = String(WebSlotManager::activeRoot()) + "/" + asset + ".tmp";
            if (!LittleFS.exists(path)) continue;
            File f = LittleFS.open(path, "r");
            if (!f) continue;
            const bool regular = !f.isDirectory();
            f.close();
            if (!regular) continue;
            Serial.printf("Web upload cleanup: %s %s\n", path.c_str(),
                          LittleFS.remove(path) ? "removed" : "failed");
            yield();
        }
    }

    // ==================================================
    // Web update - PUT
    // ==================================================

    // One staging file per allowed asset. Offset zero starts a fresh transfer.
    static bool readNumber(const String& value, uint32_t& result, uint8_t base=10)
    {
        if (!value.length() || value.length() > (base == 16 ? 8 : 10)) return false;
        result = 0;
        for (unsigned int i=0; i<value.length(); ++i) {
            char c=value[i];
            unsigned int digit = c >= '0' && c <= '9' ? c-'0' :
                c >= 'a' && c <= 'f' ? c-'a'+10 : 99;
            if (digit >= base || result > (UINT32_MAX-digit)/base) return false;
            result=result*base+digit;
        }
        return true;
    }

    static const char* firmwareBlocked(uint32_t maxBytes)
    {
        const char* reason = nullptr;
        if (!Storage::isReady()) reason = "Storage unavailable";
        else if (!WifiManager::isConnected() || WifiManager::isPortalActive()) reason = "Network unavailable";
        else if (ESP.getFlashChipSize() != ESP.getFlashChipRealSize()) reason = "Flash size mismatch";
        else if (restartPending || wifiAction || factoryResetScheduled) reason = "Maintenance action pending";
        else if (SettingsManager::savePending() || SettingsManager::saveFailed()) reason = "Settings save pending or failed";
        else if (PreviewManager::active()) reason = "Preview active";
        else if (WebUpdateManager::active()) reason = "Web update active";
        else if (OTAManager::isUpdating() || Update.isRunning()) reason = "Update already running";
        else if (!maxBytes) reason = "No firmware space";
        return reason;
    }

    // Read-only preflight. The actual upload must repeat these checks.
    static void handleFirmwarePreflight()
    {
        if (server.header("X-Wordclock-Action") != "firmware-update") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return;
        }
        if (!authorizeWebUpdate()) return;
        const uint32_t freeBytes = ESP.getFreeSketchSpace();
        // Conservative limit for the current PlatformIO d1 image layout.
        const uint32_t maxBytes = min(freeBytes & ~uint32_t(4095), uint32_t(1044464));
        uint32_t size = 0;
        if (server.hasArg("size") && (!readNumber(server.arg("size"), size) || !size)) {
            sendJson(400, "{\"error\":\"Invalid firmware size\"}"); return;
        }
        if (size > maxBytes) {
            sendJson(413, "{\"error\":\"Firmware too large\"}"); return;
        }
        const char* reason = firmwareBlocked(maxBytes);
        StaticJsonDocument<384> doc;
        doc["ready"] = reason == nullptr;
        doc["uploadSupported"] = true;
        doc["protocol"] = 2; doc["chunkSize"] = 512;
        doc["signatureRequired"] = true;
        doc["authorizationRequired"] = true;
        doc["maxBytes"] = maxBytes;
        doc["freeSketchBytes"] = freeBytes;
        if (reason) doc["reason"] = reason;
        sendJsonDocument(reason ? 409 : 200, doc);
    }

    static constexpr uint32_t FLASH_TEST_RESERVE = 4096; // retain minimum image size
    static constexpr uint32_t FW_TAIL = 256, FW_CHUNK = 512, FW_TIMEOUT = 60000;
    static br_sha256_context firmwareHash;
    static struct {
        bool active = false, verified = false;
        uint32_t size = 0, received = 0, accepted = 0, touched = 0;
        char token[33] = {};
        uint8_t digest[32] = {}, tail[FW_TAIL] = {};
    } fwSession;
    bool firmwareSessionActive() { return fwSession.active; }
    static void clearFirmwareSession() {
        if (!fwSession.active) return;
        if (Update.isRunning()) {
            if ((Update.isFinished() && !Update.hasError()) || fwSession.accepted >= fwSession.size) {
                ESP.restart(); for (;;) delay(100);
            }
            Update.end(false);
        }
        if (Update.isRunning()) { ESP.restart(); for (;;) delay(100); }
        Update.clearError();
        fwSession = {};
        FirmwareUpdate::recover(FirmwareUpdate::Owner::Web);
    }

    static int firmwareHex(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // Verify the signed digest before reserving flash or accepting image data.
    class FirmwareDigest : public UpdaterHashClass {
        const uint8_t* digest_;
    public:
        explicit FirmwareDigest(const uint8_t* digest) : digest_(digest) {}
        void begin() override {}
        void add(const void*, uint32_t) override {}
        void end() override {}
        int len() override { return 32; }
        const void* hash() override { return digest_; }
        const unsigned char* oid() override { return BR_HASH_OID_SHA256; }
    };

    static int verifyFirmwareSignature(const String& hex, const uint8_t* digest)
    {
        if (hex.length() != 512) return 422;
        uint8_t signature[256];
        for (size_t i = 0; i < sizeof(signature); ++i) {
            int hi = firmwareHex(hex[2*i]), lo = firmwareHex(hex[2*i+1]);
            if (hi < 0 || lo < 0) return 422;
            signature[i] = uint8_t((hi << 4) | lo);
        }

        static_assert(sizeof(WC_RSA_N) == 256, "RSA key size");

        // DER SubjectPublicKeyInfo for RSA-2048, exponent 65537.
        static const uint8_t prefix[] = {
            0x30, 0x82, 0x01, 0x22,
            0x30, 0x0d,
            0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
            0xf7, 0x0d, 0x01, 0x01, 0x01,
            0x05, 0x00,
            0x03, 0x82, 0x01, 0x0f, 0x00,
            0x30, 0x82, 0x01, 0x0a,
            0x02, 0x82, 0x01, 0x01, 0x00
        };
        static const uint8_t suffix[] = {
            0x02, 0x03, 0x01, 0x00, 0x01
        };

        uint8_t der[294];
        static_assert(sizeof(prefix) + sizeof(WC_RSA_N) +
                      sizeof(suffix) == sizeof(der), "DER size");

        memcpy(der, prefix, sizeof(prefix));
        memcpy(der + sizeof(prefix), WC_RSA_N, sizeof(WC_RSA_N));
        memcpy(der + sizeof(prefix) + sizeof(WC_RSA_N),
               suffix, sizeof(suffix));

        // Leave room for BearSSL's secondary stack and key parsing.
        if (ESP.getFreeHeap() < 7500 ||
            ESP.getMaxFreeBlockSize() < 6500) return 503;

        BearSSL::PublicKey key(der, sizeof(der));
        if (!key.isRSA()) return 503;
        if (ESP.getFreeHeap() < 7000 || ESP.getMaxFreeBlockSize() < 6400) return 503;

        FirmwareDigest hash(digest);

        bool valid = false;
        {
            BearSSL::SigningVerifier verifier(&key);
            valid = verifier.verify(&hash, signature, sizeof(signature));
        }

        yield();
        return valid ? 200 : 422;
    }

    // One short-lived authorization, consumed before flash staging.
    static struct {
        char token[33] = {};
        uint32_t issued = 0;
        uint32_t size = 0;
        uint8_t digest[32] = {};
    } firmwareTicket;

    static void handleFirmwareAuthorize()
    {
        if (server.header("X-Wordclock-Action") != "firmware-update") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return;
        }
        if (!authorizeWebUpdate()) return;
        firmwareTicket = {};
        const uint32_t limit = min(ESP.getFreeSketchSpace() & ~uint32_t(4095), uint32_t(1044464));
        uint32_t size = 0;
        if (server.arg("confirm") != "INSTALL" || !readNumber(server.arg("size"), size) ||
            size < 3 * FLASH_TEST_RESERVE || size > limit) {
            sendJson(400, "{\"error\":\"Invalid size or confirmation\"}"); return;
        }
        const char* reason = firmwareBlocked(limit);
        if (reason) { sendJson(409, "{\"error\":\"Firmware not ready\"}"); return; }
        const String hash = server.arg("sha256");
        uint8_t digest[32];
        if (hash.length() != 64) { sendJson(400, "{\"error\":\"Invalid SHA-256\"}"); return; }
        for (size_t i = 0; i < 32; ++i) {
            int hi = firmwareHex(hash[2*i]), lo = firmwareHex(hash[2*i+1]);
            if (hi < 0 || lo < 0) { sendJson(400, "{\"error\":\"Invalid SHA-256\"}"); return; }
            digest[i] = uint8_t((hi << 4) | lo);
        }
        int status = verifyFirmwareSignature(server.arg("signature"), digest);
        Serial.printf("Firmware authorization: %d, heap=%u, max=%u\n", status,
                      ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
        if (status != 200) {
            sendJson(status, status == 503 ? "{\"error\":\"Insufficient verification memory or key unavailable\"}" :
                "{\"error\":\"Missing or invalid firmware signature\"}"); return;
        }
        snprintf(firmwareTicket.token, sizeof(firmwareTicket.token), "%08lx%08lx%08lx%08lx",
                 (unsigned long)os_random(), (unsigned long)os_random(),
                 (unsigned long)os_random(), (unsigned long)os_random());
        firmwareTicket.issued = millis();
        firmwareTicket.size = size;
        memcpy(firmwareTicket.digest, digest, sizeof(digest));
        sendJson(200, String("{\"token\":\"") + firmwareTicket.token + "\",\"expiresIn\":60}");
    }

    static bool firmwareAuth() {
        if (server.header("X-Wordclock-Action") != "firmware-update") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return false;
        }
        return authorizeWebUpdate();
    }
    static bool firmwareSessionAuth() {
        if (!firmwareAuth()) return false;
        if (!fwSession.active || millis()-fwSession.touched >= FW_TIMEOUT ||
            server.arg("token") != fwSession.token) {
            sendJson(403, "{\"error\":\"Invalid session\"}"); return false;
        }
        return true;
    }
    static void firmwareFail(int code, const char* msg) {
        clearFirmwareSession();
        sendJson(code, String("{\"error\":\"")+msg+"\",\"installed\":false}");
    }
    static void handleFirmwareStart() {
        if (!firmwareAuth()) return;
        if (fwSession.active) { sendJson(409,"{\"error\":\"Session active\"}"); return; }
        auto ticket = firmwareTicket;
        firmwareTicket = {};
        if (!ticket.token[0] || millis()-ticket.issued >= 60000 ||
            server.arg("token") != ticket.token || server.arg("confirm") != "INSTALL") {
            sendJson(403,"{\"error\":\"Invalid authorization\"}"); return;
        }
        const uint32_t limit = min(ESP.getFreeSketchSpace() & ~uint32_t(4095), uint32_t(1044464));
        if (ESP.getFreeHeap()<7000 || ESP.getMaxFreeBlockSize()<5000) { sendJson(503,"{\"error\":\"Insufficient start memory\"}"); return; }
        if (ticket.size > limit || firmwareBlocked(limit) || !FirmwareUpdate::begin(FirmwareUpdate::Owner::Web)) {
            sendJson(409,"{\"error\":\"Firmware not ready\"}"); return;
        }
        fwSession = {}; fwSession.active = true; fwSession.size = ticket.size;
        memcpy(fwSession.digest,ticket.digest,32);
        snprintf(fwSession.token,sizeof(fwSession.token),"%08lx%08lx%08lx%08lx",
            (unsigned long)os_random(),(unsigned long)os_random(),(unsigned long)os_random(),(unsigned long)os_random());
        br_sha256_init(&firmwareHash);
        if (!Update.begin(ticket.size,U_FLASH)) { firmwareFail(503,"Flash begin failed"); return; }
        if (ESP.getFreeHeap()<5000 || ESP.getMaxFreeBlockSize()<3000) { firmwareFail(503,"Insufficient transfer memory"); return; }
        fwSession.touched=millis();
        Serial.printf("Firmware session: heap=%u max=%u\n",ESP.getFreeHeap(),ESP.getMaxFreeBlockSize());
        sendJson(200,String("{\"token\":\"")+fwSession.token+"\",\"chunkSize\":512,\"offset\":0}");
    }
    static void handleFirmwareBlock() {
        if (!firmwareSessionAuth()) return;
        uint32_t offset=0;
        const String& data=server.arg("plain");
        const size_t n=data.length();
        const bool offsetValid=readNumber(server.arg("offset"),offset);
        const size_t expected=offset<fwSession.size ? min(static_cast<size_t>(FW_CHUNK),static_cast<size_t>(fwSession.size-offset)) : 0;
        const char* error=nullptr;
        if (fwSession.verified) error="Session already verified";
        else if (!offsetValid) error="Invalid offset parameter";
        else if (offset!=fwSession.received) error="Unexpected offset";
        else if (!expected || n!=expected) error="Invalid block length";
        if (error) {
            Serial.printf("Firmware block rejected: %s offset=%lu expectedOffset=%lu length=%u expectedLength=%u heap=%u max=%u\n",
                error,(unsigned long)offset,(unsigned long)fwSession.received,(unsigned)n,(unsigned)expected,ESP.getFreeHeap(),ESP.getMaxFreeBlockSize());
            StaticJsonDocument<384> doc;
            doc["error"]=error; doc["offset"]=offset; doc["expectedOffset"]=fwSession.received;
            doc["length"]=n; doc["expectedLength"]=expected;
            doc["freeHeap"]=ESP.getFreeHeap(); doc["maxFreeBlock"]=ESP.getMaxFreeBlockSize();
            sendJsonDocument(409, doc); return;
        }
        auto* bytes=reinterpret_cast<uint8_t*>(const_cast<char*>(data.c_str()));
        if (!offset && bytes[0]!=0xe9) { firmwareFail(400,"Invalid image magic"); return; }
        const uint32_t cap=fwSession.size-FW_TAIL;
        const size_t count=min(n,static_cast<size_t>(offset<cap?cap-offset:0));
        br_sha256_update(&firmwareHash,bytes,n);
        if (count && (Update.write(bytes,count)!=count || Update.hasError())) { firmwareFail(500,"Flash write failed"); return; }
        fwSession.accepted+=count;
        if (n>count) memcpy(fwSession.tail+(offset+count-cap),bytes+count,n-count);
        fwSession.received+=n; fwSession.touched=millis();
        FirmwareUpdate::progress(FirmwareUpdate::Owner::Web,fwSession.received,fwSession.size);
        sendJson(200,String("{\"offset\":")+String(fwSession.received)+"}");
    }
    static void handleFirmwareStatus() {
        if (!firmwareSessionAuth()) return;
        StaticJsonDocument<256> doc;
        doc["offset"]=fwSession.received; doc["size"]=fwSession.size; doc["verified"]=fwSession.verified;
        doc["freeHeap"]=ESP.getFreeHeap(); doc["maxFreeBlock"]=ESP.getMaxFreeBlockSize();
        sendJsonDocument(200, doc);
    }
    static void handleFirmwareVerify() {
        if (!firmwareSessionAuth()) return;
        if (fwSession.verified) { sendJson(200,"{\"verified\":true}"); return; }
        if (fwSession.received!=fwSession.size || fwSession.accepted!=fwSession.size-FW_TAIL || !Update.isRunning() || Update.hasError()) {
            firmwareFail(409,"Incomplete image"); return;
        }
        uint8_t digest[32]; br_sha256_out(&firmwareHash,digest);
        if (memcmp(digest,fwSession.digest,32)) { firmwareFail(422,"SHA-256 mismatch"); return; }
        fwSession.verified=true; fwSession.touched=millis(); sendJson(200,"{\"verified\":true}");
    }
    static void handleFirmwareActivate() {
        if (!firmwareSessionAuth()) return;
        if (!fwSession.verified || server.arg("confirm")!="INSTALL") { sendJson(409,"{\"error\":\"Not verified\"}"); return; }
        if (Update.write(fwSession.tail,FW_TAIL)!=FW_TAIL || Update.hasError() || !Update.isFinished() || !Update.end(false)) {
            // Never call end again when the final bytes may have been accepted.
            sendJson(500,"{\"error\":\"Finalization failed\"}"); delay(300); ESP.restart(); for (;;) delay(100);
        }
        FirmwareUpdate::validated(FirmwareUpdate::Owner::Web);
        sendJson(202,"{\"sha256Verified\":true,\"installationScheduled\":true}");
        delay(1500); ESP.restart(); for (;;) delay(100);
    }
    static void handleFirmwareCancel() {
        if (!firmwareSessionAuth()) return;
        clearFirmwareSession(); sendJson(200,"{\"cancelled\":true}");
    }

    static bool releaseAuthorized() {
        if (server.header("X-Wordclock-Action") != "web-update") { sendJson(403,"{\"error\":\"Action required\"}"); return false; }
        return authorizeWebUpdate();
    }
    static void handleReleaseStatus() {
        if (!releaseAuthorized()) return;
        const auto state=WebUpdateManager::snapshot();
        StaticJsonDocument<512> doc;
        doc["active"]=WebUpdateManager::active(); doc["id"]=state.id;
        doc["target"]=state.target==WebSlotManager::Slot::A?"A":"B";
        doc["receivedBytes"]=state.received; doc["totalBytes"]=state.totalBytes;
        doc["completedFiles"]=state.completed; doc["fileCount"]=state.fileCount;
        doc["complete"]=state.id && state.completed==state.fileCount && state.received==state.totalBytes;
        doc["verified"]=state.verified; doc["verifiedFiles"]=state.verifiedFiles;
        if(state.lastError) doc["error"]=state.lastError;
        sendJsonDocument(200, doc);
    }
    static void handleReleaseChunk() {
        if (!releaseAuthorized()) return;
        uint32_t id=0, offset=0;
        if (!readNumber(server.arg("id"),id) || !readNumber(server.arg("offset"),offset)) { sendJson(400,"{\"error\":\"parameters\"}"); return; }
        const String& name=server.arg("name");
        const String& data=server.arg("plain");
        if (!WebUpdateManager::chunk(id,name.c_str(),offset,reinterpret_cast<const uint8_t*>(data.c_str()),data.length())) { sendJson(409,"{\"error\":\"chunk-rejected\"}"); return; }
        sendJson(200,String("{\"offset\":")+String(offset+data.length())+"}");
    }
    static void handleReleaseVerify() {
        if (!releaseAuthorized()) return;
        uint32_t id=0;
        if (!readNumber(server.arg("id"),id)) { sendJson(400,"{\"error\":\"parameters\"}"); return; }
        const char* error=WebUpdateManager::verify(id);
        if (error) { sendJson(strcmp(error,"sha256-mismatch")==0?422:409,String("{\"verified\":false,\"error\":\"")+error+"\"}"); return; }
        sendJson(200,"{\"verified\":true,\"verifiedFiles\":12}");
    }
    static void handleReleaseSwitch() {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        if (!releaseAuthorized()) return;
        if (server.arg("confirm") != "SWITCH") { sendJson(400,"{\"error\":\"confirmation-required\"}"); return; }
        const bool rollback = server.uri() == "/api/system/web-update/rollback";
        uint32_t id=0;
        if (!rollback && !readNumber(server.arg("id"),id)) { sendJson(400,"{\"error\":\"parameters\"}"); return; }
        const char* error=rollback ? WebUpdateManager::rollback() : WebUpdateManager::activate(id);
        if (error) { sendJson(409,String("{\"error\":\"")+error+"\"}"); return; }
        sendJson(200,String("{\"activated\":true,\"webSlot\":\"")+(WebSlotManager::activeSlot()==WebSlotManager::Slot::A?"A":"B")+"\",\"generation\":"+String(WebSlotManager::selectionGeneration())+"}");
    }
    static void handleReleaseCancel() {
        if (!releaseAuthorized()) return;
        uint32_t id=0;
        if (!readNumber(server.arg("id"),id) || !WebUpdateManager::cancel(id)) { sendJson(409,"{\"error\":\"cancel-rejected\"}"); return; }
        sendJson(200,"{\"cancelled\":true}");
    }

    static void handleWebUpdate()
    {
        if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
        if (!authorizeWebUpdate()) return;
        if (WebUpdateManager::active()) { sendJson(409, "{\"error\":\"Web update active\"}"); return; }
        if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
        String name=server.arg("name");
        if (!isAllowedWebFile(name)) { sendJson(400, "{\"error\":\"File not allowed\"}"); return; }
        if (WebSlotManager::isIncomplete(WebSlotManager::activeSlot())) { sendJson(409, "{\"error\":\"Web interface incomplete\"}"); return; }
        const char* root = WebSlotManager::activeRoot();
        const char* slot = WebSlotManager::activeSlot() == WebSlotManager::Slot::A ? "A" : "B";
        if (server.hasArg("slot") && server.arg("slot") != slot) {
            sendJson(409, "{\"error\":\"Active slot changed\"}"); return;
        }
        String path=String(root)+"/"+name, temporaryPath=path+".tmp";
        uint32_t offset=0, total=0, expectedCrc=0;
        if (!readNumber(server.arg("offset"),offset) || !readNumber(server.arg("total"),total) ||
            !readNumber(server.arg("crc"),expectedCrc,16) || !total || total>32768u || offset>total) {
            sendJson(400,"{\"error\":\"Invalid transfer parameters\"}"); return;
        }
        if (server.arg("action")=="commit") {
            File f=LittleFS.open(temporaryPath,"r");
            if (!f || f.size()!=total || offset!=total) { sendJson(409,"{\"error\":\"Incomplete staging file\"}"); return; }
            uint32_t crc=0xffffffffu; size_t count=0;
            uint8_t buffer[256];
            while (count<total) {
                size_t n=f.read(buffer, sizeof(buffer));
                if (!n) break;
                count+=n;
                for (size_t i=0;i<n;++i) {
                    crc ^= buffer[i];
                    for (int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1)?0xedb88320u:0u);
                }
                yield();
            }
            f.close();
            if (count!=total || (crc^0xffffffffu)!=expectedCrc) { sendJson(409,"{\"error\":\"CRC or size mismatch\"}"); return; }
            // Mark before replacing any active asset, including on interrupted updates.
            const String marker = String(root) + "/.development";
            if (!LittleFS.exists(marker)) {
                File dirty = LittleFS.open(marker, "w");
                if (!dirty) { sendJson(500,"{\"error\":\"Cannot mark development slot\"}"); return; }
                const size_t marked = dirty.write(static_cast<uint8_t>(1));
                dirty.close();
                if (marked != 1 || !LittleFS.exists(marker)) { sendJson(500,"{\"error\":\"Cannot mark development slot\"}"); return; }
            }
            if (!LittleFS.rename(temporaryPath,path)) { sendJson(500,"{\"error\":\"Cannot replace file\"}"); return; }
            sendJson(200,String("{\"ok\":true,\"size\":")+String(total)+"}"); return;
        }
        if (server.arg("action")!="chunk") { sendJson(400,"{\"error\":\"Unknown transfer action\"}"); return; }
        const String& content=server.arg("plain");
        size_t length=content.length();
        if (!length || length>1024 || length>total-offset) { sendJson(413,"{\"error\":\"Invalid chunk size\"}"); return; }
        if (!LittleFS.exists(root) && !LittleFS.mkdir(root)) { sendJson(500,"{\"error\":\"Cannot create web directory\"}"); return; }
        File f=LittleFS.open(temporaryPath,offset==0?"w":"a");
        if (!f) { sendJson(500,"{\"error\":\"Cannot open staging file\"}"); return; }
        if (f.size()!=offset) { f.close(); sendJson(409,"{\"error\":\"Offset mismatch; restart transfer\"}"); return; }
        size_t written=f.write(reinterpret_cast<const uint8_t*>(content.c_str()),length);
        f.close();
        if (written!=length) { sendJson(500,"{\"error\":\"Incomplete chunk write\"}"); return; }
        sendJson(200,String("{\"ok\":true,\"offset\":")+String(offset+length)+"}");
    }

    // ==================================================
    // Web update - Status
    // ==================================================

    static void handleWebUpdateStatus()
    {

        if (!authorizeWebUpdate())
        {
            return;
        }

        if (!Storage::isReady())
        {

            sendJson(
                503,
                "{\"error\":\"Storage unavailable\"}");

            return;
        }

        String json =
            "{\"enabled\":true,\"maxFileSize\":";

        json += String(
            32768u);

        json += ",\"chunkSize\":1024,\"protocol\":2,\"activeSlot\":\"";
        json += WebSlotManager::activeSlot() == WebSlotManager::Slot::A ? "A" : "B";
        json += "\",\"development\":";
        json += LittleFS.exists(String(WebSlotManager::activeRoot()) + "/.development") ? "true" : "false";
        json += "}";

        sendJson(200, json);
    }

    static void handleNotFound()
    {

        sendJson(
            404,
            "{\"error\":\"Not found\"}");
    }

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {
        cleanupAbandonedWebUploads();

        server.on(
            "/",
            HTTP_GET,
            handleRoot);

        server.on(
            "/style.css",
            HTTP_GET,
            []()
            {
                serveWebFile(
                    "style.css",
                    "text/css; charset=utf-8");
            });

        server.on(
            "/app.js",
            HTTP_GET,
            []()
            {
                serveWebFile(
                    "app.js",
                    "application/javascript; charset=utf-8");
            });

        server.on("/icon.svg", HTTP_GET, []() {
            serveWebFile("icon.svg", "image/svg+xml");
        });
        server.on("/manifest.json", HTTP_GET, []() {
            serveWebFile("manifest.json", "application/manifest+json");
        });

        server.on("/night.js", HTTP_GET, []() {
            serveWebFile("night.js", "application/javascript; charset=utf-8");
        });

        server.on("/theme-rules.js", HTTP_GET, []() {
            serveWebFile("theme-rules.js", "application/javascript; charset=utf-8");
        });

        server.on("/studio-visual.js", HTTP_GET, []() {
            serveWebFile("studio-visual.js", "application/javascript; charset=utf-8");
        });

        server.on("/studio.js", HTTP_GET, []() {
            serveWebFile("studio.js", "application/javascript; charset=utf-8");
        });

        server.on("/maintenance.js", HTTP_GET, []() {
            serveWebFile("maintenance.js", "application/javascript; charset=utf-8");
        });
        server.on("/setup.js", HTTP_GET, []() {
            serveWebFile("setup.js", "application/javascript; charset=utf-8");
        });
        server.on("/community.js", HTTP_GET, []() {
            serveWebFile("community.js", "application/javascript; charset=utf-8");
        });

        server.on(
            "/api/status",
            HTTP_GET,
            handleStatus);

        server.on("/api/device", HTTP_GET, handleGetDevice);
        server.on("/api/device", HTTP_POST, handleSetDevice);
        server.on("/api/appearance", HTTP_GET, handleGetAppearance);
        server.on("/api/appearance", HTTP_POST, handleSetAppearance);

        server.on(
            "/api/theme",
            HTTP_GET,
            handleGetTheme);

        server.on(
            "/api/theme",
            HTTP_POST,
            handleSetTheme);

        server.on(
            "/api/themes",
            HTTP_GET,
            handleGetThemes);

        server.on(
            "/api/themes/apply",
            HTTP_POST,
            handleApplyTheme);

        server.on("/api/themes/custom", HTTP_GET, []() {
            if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
            String json;
            if (!server.hasArg("id")) {
                if (!CustomThemeStore::listIds(json)) { sendJson(500, "{\"error\":\"List failed\"}"); return; }
            } else {
                String id = server.arg("id");
                if (!CustomThemeStore::validCustomId(id)) {
                    sendJson(400, "{\"error\":\"Invalid custom theme ID\"}"); return;
                }
                ThemeCodec::Entry entry;
                if (!CustomThemeStore::load(id, entry)) { sendJson(404, "{\"error\":\"Theme not found or unreadable\"}"); return; }
                if (!ThemeCodec::encode(entry, json, responseThemeVersion())) { sendJson(500, "{\"error\":\"Encoding failed\"}"); return; }
            }
            sendJson(200, json);
        });

        server.on("/api/themes/custom", HTTP_POST, []() {
            if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
            if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
            if (!server.hasArg("plain")) { sendJson(400, "{\"error\":\"JSON body required\"}"); return; }
            String body = server.arg("plain");
            if (body.length() > ThemeCodec::MAX_BYTES) { sendJson(413, "{\"error\":\"Theme too large\"}"); return; }
            ThemeCodec::Entry entry;
            if (!ThemeCodec::decode(body, entry, true)) { sendJson(400, "{\"error\":\"Invalid theme\"}"); return; }
            if (CustomThemeStore::count() >= CustomThemeStore::MAX_THEMES) { sendJson(409, "{\"error\":\"Theme limit reached\"}"); return; }
            if (!CustomThemeStore::create(entry)) { sendJson(500, "{\"error\":\"Creation failed\"}"); return; }
            String json;
            if (!ThemeCodec::encode(entry, json, responseThemeVersion())) { sendJson(500, "{\"error\":\"Theme saved but response encoding failed\"}"); return; }
            sendJson(201, json);
        });

        server.on("/api/themes/custom", HTTP_PUT, []() {
            if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
            if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
            String id = server.arg("id");
            if (!CustomThemeStore::validCustomId(id)) { sendJson(400, "{\"error\":\"Invalid custom theme ID\"}"); return; }
            if (!server.hasArg("plain")) { sendJson(400, "{\"error\":\"JSON body required\"}"); return; }
            String body = server.arg("plain");
            if (body.length() > ThemeCodec::MAX_BYTES) { sendJson(413, "{\"error\":\"Theme too large\"}"); return; }
            ThemeCodec::Entry entry;
            if (!ThemeCodec::decode(body, entry, false)) { sendJson(400, "{\"error\":\"Invalid theme or missing ID\"}"); return; }
            if (entry.id != id) { sendJson(400, "{\"error\":\"Theme ID mismatch\"}"); return; }
            entry.id = id;
            {
                ThemeCodec::Entry original;
                if (!CustomThemeStore::load(id, original)) { sendJson(404, "{\"error\":\"Theme not found or unreadable\"}"); return; }
                if (entry.sourceVersion == 2) {
                    entry.theme.frontBrightness = original.theme.frontBrightness;
                    entry.theme.rearBrightness = original.theme.rearBrightness;
                }
            }
            String json;
            if (!ThemeCodec::encode(entry, json, responseThemeVersion())) { sendJson(400, "{\"error\":\"Invalid theme\"}"); return; }
            if (!CustomThemeStore::update(entry)) { sendJson(500, "{\"error\":\"Update failed\"}"); return; }
            sendJson(200, json);
        });

        server.on("/api/themes/custom", HTTP_DELETE, []() {
            if (firmwareSessionActive()) { sendJson(409,"{\"error\":\"Firmware session active\"}"); return; }
            if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
            String id = server.arg("id");
            if (!CustomThemeStore::validCustomId(id)) { sendJson(400, "{\"error\":\"Invalid custom theme ID\"}"); return; }
            String path = String("/themes/custom/") + id + ".json";
            if (!LittleFS.exists(path)) { sendJson(404, "{\"error\":\"Theme not found\"}"); return; }
            if (!CustomThemeStore::remove(id)) { sendJson(500, "{\"error\":\"Deletion failed\"}"); return; }
            sendJson(200, "{\"ok\":true}");
        });

        server.on("/api/preview", HTTP_POST, handlePreview);
        server.on("/api/night", HTTP_GET, handleGetNight);
        server.on("/api/night", HTTP_POST, handleSetNight);
        server.on("/api/transitions", HTTP_GET, handleGetTransitions);
        server.on("/api/transitions", HTTP_POST, handleSetTransitions);
        server.on("/api/display", HTTP_GET, handleDisplayFrame);
        server.on("/api/weather", HTTP_GET, handleGetWeather);
        server.on("/api/weather", HTTP_POST, handleSetWeather);
        server.on("/api/automations", HTTP_GET, handleGetRules);
        server.on("/api/automations", HTTP_POST, handleSetRules);
        server.on("/api/animations", HTTP_GET, handleGetAnimations);
        server.on("/api/animations", HTTP_POST, handleSetAnimations);
        server.on("/api/animations/test", HTTP_POST, handleTestAnimation);

        if (true)
        {


            server.on("/api/system/web-update/preflight", HTTP_POST,
                      handleWebUpdatePreflight, handleManifestChunk);
            server.on("/api/system/web-update/start", HTTP_POST, handleWebUpdatePreflight, handleManifestChunk);
            server.on("/api/system/web-update/status", HTTP_GET, handleReleaseStatus);
            server.on("/api/system/web-update/chunk", HTTP_PUT, handleReleaseChunk);
            server.on("/api/system/web-update/cancel", HTTP_POST, handleReleaseCancel);
            server.on("/api/system/web-update/verify", HTTP_POST, handleReleaseVerify);
            server.on("/api/system/web-update/activate", HTTP_POST, handleReleaseSwitch);
            server.on("/api/system/web-update/rollback", HTTP_POST, handleReleaseSwitch);
            server.on("/recovery", HTTP_GET, []() {
                server.sendHeader("Cache-Control", "no-store");
                server.send(200,"text/html; charset=utf-8", R"HTML(<!doctype html><html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Wordclock - Secours</title><body><h1>Wordclock - Secours</h1><p>Retour a l'interface precedente, si disponible.</p><button id="back">Restaurer</button><pre id="result"></pre><p><a href="/">Ouvrir Wordclock</a></p><script>document.getElementById('back').onclick=async()=>{if(!confirm('Restaurer la version precedente ?'))return;const p=prompt('Mot de passe administrateur');if(p===null)return;try{const r=await fetch('/api/system/web-update/rollback?confirm=SWITCH',{method:'POST',headers:{'X-Wordclock-Action':'web-update','Authorization':'Basic '+btoa(Array.from(new TextEncoder().encode('admin:'+p),x=>String.fromCharCode(x)).join(''))}});document.getElementById('result').textContent=await r.text();}catch(e){document.getElementById('result').textContent=String(e);}};</script></body></html>)HTML");
            });

            server.on(
                "/api/dev/web",
                HTTP_GET,
                handleWebUpdateStatus);

            server.on(
                "/api/dev/web",
                HTTP_PUT,
                handleWebUpdate);
        }

        server.on("/api/system/firmware", HTTP_GET, handleFirmwarePreflight);
        server.on("/api/system/firmware/authorize", HTTP_POST, handleFirmwareAuthorize);
        server.on("/api/system/firmware/start", HTTP_POST, handleFirmwareStart);
        server.on("/api/system/firmware/chunk", HTTP_PUT, handleFirmwareBlock);
        server.on("/api/system/firmware/status", HTTP_GET, handleFirmwareStatus);
        server.on("/api/system/firmware/verify", HTTP_POST, handleFirmwareVerify);
        server.on("/api/system/firmware/activate", HTTP_POST, handleFirmwareActivate);
        server.on("/api/system/firmware/cancel", HTTP_POST, handleFirmwareCancel);
        server.on("/api/system/firmware/install", HTTP_POST, [](){ sendJson(410,"{\"error\":\"Use firmware protocol 2\"}"); });
        server.on("/api/system", HTTP_GET, handleSystemInfo);
        server.on("/api/system/restart", HTTP_POST, handleRestart);
        server.on("/api/system/reset-settings", HTTP_POST, handleResetSettings);
        server.on("/api/system/factory-reset", HTTP_POST, handleFactoryReset);
        server.on("/api/clock", HTTP_GET, handleClockInfo);
        server.on("/api/clock", HTTP_POST, handleClockSet);
        server.on("/api/wifi", HTTP_GET, handleWifiInfo);
        server.on("/api/wifi", HTTP_POST, handleWifiAction);
        // Read-only handoff probe; only the setup portal gets cross-origin access.
        server.on("/api/handoff-ready", HTTP_GET, []() {
            server.sendHeader("Access-Control-Allow-Origin", "http://192.168.4.1");
            if (!WifiManager::isConnected() || WifiManager::isPortalActive()) {
                sendJson(503, "{\"ready\":false}");
                return;
            }
            sendJson(200, String("{\"ready\":true,\"device\":\"") + String(ESP.getChipId(), HEX) + "\"}");
        });
        server.on("/api/handoff-ready", HTTP_OPTIONS, []() {
            server.sendHeader("Access-Control-Allow-Origin", "http://192.168.4.1");
            server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
            server.sendHeader("Access-Control-Allow-Private-Network", "true");
            server.send(204, "text/plain", "");
        });
        server.collectHeaders("X-Wordclock-Action", "Accept-Encoding");
        server.onNotFound(handleNotFound);
    }

    // ==================================================
    // Server update
    // ==================================================

    void stop()
    {
        if (running) {
            server.stop();
            running = false;
            Serial.println("Web server stopped");
        }
        if (mdnsRunning) {
            MDNS.close();
            mdnsRunning = false;
            Serial.println("mDNS stopped");
        }
        mdnsAttempted = false;
    }

    void update()
    {
        sampleMemory();
        if (fwSession.active && (millis()-fwSession.touched >= FW_TIMEOUT || !WifiManager::isConnected() || WifiManager::isPortalActive())) clearFirmwareSession();

        if (restartPending) {
            if (millis() - restartAt >= 1500) {
                if (factoryResetScheduled || (!SettingsManager::savePending() && !SettingsManager::saveFailed())) {
                    Serial.println("Maintenance restart");
                    ESP.restart();
                    return;
                }
                restartPending = false;
                Serial.println("Maintenance restart cancelled: settings pending");
            }
            return;
        }
        if (wifiAction && millis() - wifiActionAt >= 1200) {
            const uint8_t action = wifiAction;
            wifiAction = 0;
            if (action == 2) WifiManager::forgetNetwork();
            else WifiManager::startConfigPortal();
        }

        bool available =
            (WifiManager::isConnected() || WifiManager::isOfflineAP()) &&
            !WifiManager::isPortalActive();

        if (!available)
        {

            stop();

            return;
        }

        if (!running)
        {

            server.begin();

            running = true;

            Serial.println("Web server started");

            Serial.println(
                WifiManager::getIPAddress());
        }

        const IPAddress currentAddress = WiFi.localIP();
        if (mdnsRunning && (!WifiManager::isConnected() || currentAddress != mdnsAddress)) {
            MDNS.close();
            mdnsRunning = false;
            mdnsAttempted = false;
            Serial.println("mDNS restarting: IP changed");
        }

        if (WifiManager::isConnected() && !mdnsRunning && (!mdnsAttempted ||
                millis() - mdnsAttemptAt >= MDNS_RETRY_MS)) {
            mdnsAttempted = true;
            mdnsAttemptAt = millis();
            if (MDNS.begin("wordclock")) {
                MDNS.addService("http", "tcp", 80);
                mdnsAddress = currentAddress;
                mdnsRunning = true;
                Serial.println("http://wordclock.local");
            } else {
                MDNS.close();
                Serial.println("mDNS failed; retry in 10s");
            }
        }

        server.handleClient();
        if (manifestRx.seen || manifestRx.data) clearManifestRx();
        if (mdnsRunning)
        {
            MDNS.update();
        }
    }

}
