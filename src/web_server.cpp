
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <ctype.h>
#include <stdlib.h>

#include "web_server.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "storage.h"
#include "theme_manager.h"
#include "theme_library.h"
#include "color_manager.h"
#include "web_update_config.h"
#include "transition_manager.h"
#include "night_manager.h"
#include "custom_theme_store.h"
#include "preview_manager.h"
#include "settings_manager.h"

namespace WebServer
{

    static ESP8266WebServer server(80);

    static bool running = false;
    static bool mdnsRunning = false;
    static uint8_t wifiAction = 0;
    static uint32_t wifiActionAt = 0;
    static bool restartPending = false;
    static uint32_t restartAt = 0;

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

    // ==================================================
    // Static files
    // ==================================================

    static void serveFile(
        const char *path,
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

        File file = LittleFS.open(path, "r");

        if (!file)
        {

            server.send(
                404,
                "text/plain",
                "File not found");

            return;
        }

        server.sendHeader(
            "Cache-Control",
            "no-store");

        server.streamFile(file, contentType);

        file.close();
    }

    static void handleRoot()
    {

        serveFile(
            "/www/index.html",
            "text/html; charset=utf-8");
    }

    // ==================================================
    // System status
    // ==================================================

    static void handleStatus()
    {

        TimeManager::ClockTime now;

        bool valid = TimeManager::getTime(now);

        char timeString[9] = "";

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
            "\"firmware\":\"3.0.0-dev\","
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
    static void handlePreview() {
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
        String value = server.arg("duration");
        if (value.length() == 0 || value.length() > 4) { sendJson(400, "{\"error\":\"Invalid duration\"}"); return; }
        for (unsigned i = 0; i < value.length(); ++i)
            if (value[i] < '0' || value[i] > '9') { sendJson(400, "{\"error\":\"Invalid duration\"}"); return; }
        long duration = value.toInt();
        if (duration > TransitionManager::MAX_DURATION) { sendJson(400, "{\"error\":\"Invalid duration\"}"); return; }
        TransitionManager::setDuration(static_cast<uint16_t>(duration));
        handleGetTransitions();
    }

    static void handleGetDevice() {
        const auto value = SettingsManager::deviceSettings();
        StaticJsonDocument<512> doc;
        doc["name"] = value.name;
        doc["timezone"] = value.timezone;
        doc["setupComplete"] = value.setupComplete;
        doc["savePending"] = SettingsManager::savePending();
        doc["saveFailed"] = SettingsManager::saveFailed();
        String json; serializeJson(doc, json);
        sendJson(200, json);
    }
    static void handleSetDevice() {
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

    static void handleGetThemes()
    {
        String factory = ThemeLibrary::getCatalog();
        int closing = factory.lastIndexOf(']');
        if (closing < 0) {
            sendJson(500, "{\"error\":\"Factory catalog unavailable\"}");
            return;
        }
        String ids;
        DynamicJsonDocument list(3072);
        if (Storage::isReady()) {
            if (!CustomThemeStore::listIds(ids) || deserializeJson(list, ids) || !list.is<JsonArray>()) {
                sendJson(503, "{\"error\":\"Custom catalog unavailable\"}");
                return;
            }
        }
        // Stream each entry separately; never allocate the full merged catalog.
        factory.remove(closing);
        server.sendHeader("Cache-Control", "no-store");
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "application/json", "");
        server.sendContent(factory);
        factory = String();
        ids = String();
        for (JsonVariantConst item : list.as<JsonArrayConst>()) {
            if (!server.client().connected()) return;
            ThemeCodec::Entry entry;
            String encoded;
            if (!CustomThemeStore::load(item.as<String>(), entry) || !ThemeCodec::encode(entry, encoded)) {
                Serial.println("Custom catalog: unreadable entry skipped");
                continue;
            }
            server.sendContent(",");
            server.sendContent(encoded);
            yield();
        }
        server.sendContent("]");
        server.sendContent("");
    }

    static void handleApplyTheme()
    {

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
            !WebUpdateConfig::ENABLED ||
            strcmp(
                WebUpdateConfig::PASSWORD,
                "CHANGE_ME") == 0)
        {

            sendJson(
                403,
                "{\"error\":\"Web update disabled\"}");

            return false;
        }

        if (!server.authenticate(
                WebUpdateConfig::USERNAME,
                WebUpdateConfig::PASSWORD))
        {

            server.requestAuthentication();

            return false;
        }

        return true;
    }

    static void handleWifiInfo()
    {
        DynamicJsonDocument doc(512);
        doc["ssid"] = WiFi.SSID();
        doc["connected"] = WifiManager::isConnected();
        doc["ip"] = WiFi.localIP().toString();
        doc["hostname"] = "wordclock.local";
        String json;
        serializeJson(doc, json);
        sendJson(200, json);
    }

    static void handleSystemInfo()
    {
        DynamicJsonDocument doc(768);
        doc["firmware"] = "3.0.0-dev";
        doc["hostname"] = "wordclock.local";
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["flashBytes"] = ESP.getFlashChipRealSize();
        doc["storageReady"] = Storage::isReady();
        FSInfo info;
        if (Storage::isReady() && LittleFS.info(info)) {
            doc["storageTotal"] = info.totalBytes;
            doc["storageUsed"] = info.usedBytes;
        }
        doc["savePending"] = SettingsManager::savePending();
        doc["saveFailed"] = SettingsManager::saveFailed();
        doc["restartPending"] = restartPending;
        if (doc.overflowed()) { sendJson(503, "{\"error\":\"Insufficient memory\"}"); return; }
        String json;
        serializeJson(doc, json);
        sendJson(200, json);
    }

    static void handleRestart()
    {
        if (server.header("X-Wordclock-Action") != "restart") {
            sendJson(403, "{\"error\":\"Missing action header\"}"); return;
        }
        if (!authorizeWebUpdate()) return;
        if (restartPending || wifiAction || SettingsManager::savePending() || SettingsManager::saveFailed()) {
            sendJson(409, "{\"error\":\"Action or settings save pending\"}"); return;
        }
        if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
        restartPending = true;
        restartAt = millis();
        sendJson(202, "{\"ok\":true,\"restartPending\":true}");
    }

    static void handleWifiAction()
    {
        // Custom header prevents cross-origin HTML form submissions.
        if (server.header("X-Wordclock-Action") != "wifi") {
            sendJson(403, "{\"error\":\"Missing action header\"}");
            return;
        }
        if (!authorizeWebUpdate()) return;
        if (wifiAction || restartPending) { sendJson(409, "{\"error\":\"Action pending\"}"); return; }
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

    static bool isAllowedWebFile(
        const String &name)
    {

        return name == "index.html" ||
               name == "style.css" ||
               name == "app.js" ||
               name == "night.js" ||
               name == "studio.js" ||
               name == "theme-rules.js" ||
               name == "studio-visual.js" ||
               name == "community.js" ||
               name == "setup.js" ||
               name == "icon.svg" ||
               name == "manifest.json";
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

    static void handleWebUpdate()
    {
        if (!authorizeWebUpdate()) return;
        if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
        String name=server.arg("name");
        if (!isAllowedWebFile(name)) { sendJson(400, "{\"error\":\"File not allowed\"}"); return; }
        String path="/www/"+name, temporaryPath=path+".tmp";
        uint32_t offset=0, total=0, expectedCrc=0;
        if (!readNumber(server.arg("offset"),offset) || !readNumber(server.arg("total"),total) ||
            !readNumber(server.arg("crc"),expectedCrc,16) || !total || total>WebUpdateConfig::MAX_FILE_SIZE || offset>total) {
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
            if (!LittleFS.rename(temporaryPath,path)) { sendJson(500,"{\"error\":\"Cannot replace file\"}"); return; }
            sendJson(200,String("{\"ok\":true,\"size\":")+String(total)+"}"); return;
        }
        if (server.arg("action")!="chunk") { sendJson(400,"{\"error\":\"Unknown transfer action\"}"); return; }
        const String content=server.arg("plain");
        size_t length=content.length();
        if (!length || length>1024 || length>total-offset) { sendJson(413,"{\"error\":\"Invalid chunk size\"}"); return; }
        if (!LittleFS.exists("/www") && !LittleFS.mkdir("/www")) { sendJson(500,"{\"error\":\"Cannot create web directory\"}"); return; }
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
            WebUpdateConfig::MAX_FILE_SIZE);

        json += ",\"chunkSize\":1024,\"protocol\":2}";

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

        server.on(
            "/",
            HTTP_GET,
            handleRoot);

        server.on(
            "/style.css",
            HTTP_GET,
            []()
            {
                serveFile(
                    "/www/style.css",
                    "text/css; charset=utf-8");
            });

        server.on(
            "/app.js",
            HTTP_GET,
            []()
            {
                serveFile(
                    "/www/app.js",
                    "application/javascript; charset=utf-8");
            });

        server.on("/icon.svg", HTTP_GET, []() {
            serveFile("/www/icon.svg", "image/svg+xml");
        });
        server.on("/manifest.json", HTTP_GET, []() {
            serveFile("/www/manifest.json", "application/manifest+json");
        });

        server.on("/night.js", HTTP_GET, []() {
            serveFile("/www/night.js", "application/javascript; charset=utf-8");
        });

        server.on("/theme-rules.js", HTTP_GET, []() {
            serveFile("/www/theme-rules.js", "application/javascript; charset=utf-8");
        });

        server.on("/studio-visual.js", HTTP_GET, []() {
            serveFile("/www/studio-visual.js", "application/javascript; charset=utf-8");
        });

        server.on("/studio.js", HTTP_GET, []() {
            serveFile("/www/studio.js", "application/javascript; charset=utf-8");
        });

        server.on("/setup.js", HTTP_GET, []() {
            serveFile("/www/setup.js", "application/javascript; charset=utf-8");
        });
        server.on("/community.js", HTTP_GET, []() {
            serveFile("/www/community.js", "application/javascript; charset=utf-8");
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
                if (!ThemeCodec::encode(entry, json)) { sendJson(500, "{\"error\":\"Encoding failed\"}"); return; }
            }
            sendJson(200, json);
        });

        server.on("/api/themes/custom", HTTP_POST, []() {
            if (!Storage::isReady()) { sendJson(503, "{\"error\":\"Storage unavailable\"}"); return; }
            if (!server.hasArg("plain")) { sendJson(400, "{\"error\":\"JSON body required\"}"); return; }
            String body = server.arg("plain");
            if (body.length() > ThemeCodec::MAX_BYTES) { sendJson(413, "{\"error\":\"Theme too large\"}"); return; }
            ThemeCodec::Entry entry;
            if (!ThemeCodec::decode(body, entry, true)) { sendJson(400, "{\"error\":\"Invalid theme\"}"); return; }
            if (CustomThemeStore::count() >= CustomThemeStore::MAX_THEMES) { sendJson(409, "{\"error\":\"Theme limit reached\"}"); return; }
            if (!CustomThemeStore::create(entry)) { sendJson(500, "{\"error\":\"Creation failed\"}"); return; }
            String json;
            if (!ThemeCodec::encode(entry, json)) { sendJson(500, "{\"error\":\"Theme saved but response encoding failed\"}"); return; }
            sendJson(201, json);
        });

        server.on("/api/themes/custom", HTTP_PUT, []() {
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
            }
            String json;
            if (!ThemeCodec::encode(entry, json)) { sendJson(400, "{\"error\":\"Invalid theme\"}"); return; }
            if (!CustomThemeStore::update(entry)) { sendJson(500, "{\"error\":\"Update failed\"}"); return; }
            sendJson(200, json);
        });

        server.on("/api/themes/custom", HTTP_DELETE, []() {
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

        if (WebUpdateConfig::ENABLED)
        {

            server.on(
                "/api/dev/web",
                HTTP_GET,
                handleWebUpdateStatus);

            server.on(
                "/api/dev/web",
                HTTP_PUT,
                handleWebUpdate);
        }

        server.on("/api/system", HTTP_GET, handleSystemInfo);
        server.on("/api/system/restart", HTTP_POST, handleRestart);
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
        server.collectHeaders("X-Wordclock-Action");
        server.onNotFound(handleNotFound);
    }

    // ==================================================
    // Server update
    // ==================================================

    void update()
    {

        if (restartPending) {
            if (millis() - restartAt >= 1500) {
                if (!SettingsManager::savePending() && !SettingsManager::saveFailed()) {
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
            WifiManager::isConnected() &&
            !WifiManager::isPortalActive();

        if (!available)
        {

            if (running)
            {
                server.stop();
                running = false;
            }

            if (mdnsRunning)
            {
                MDNS.close();
                mdnsRunning = false;
            }

            return;
        }

        if (!running)
        {

            server.begin();

            running = true;

            Serial.println("Web server started");

            if (MDNS.begin("wordclock"))
            {

                MDNS.addService(
                    "http",
                    "tcp",
                    80);

                mdnsRunning = true;

                Serial.println(
                    "http://wordclock.local");
            }
            else
            {

                Serial.println("mDNS failed");
            }

            Serial.println(
                WifiManager::getIPAddress());
        }

        server.handleClient();

        if (mdnsRunning)
        {
            MDNS.update();
        }
    }

}
