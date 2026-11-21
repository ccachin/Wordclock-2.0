
#include <Arduino.h>
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

namespace WebServer
{

    static ESP8266WebServer server(80);

    static bool running = false;
    static bool mdnsRunning = false;

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
            "\"brightness\":%u,"
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

        sendJson(
            200,
            ThemeLibrary::getCatalog());
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

    // ==================================================
    // Web update - Allowed files
    // ==================================================

    static bool isAllowedWebFile(
        const String &name)
    {

        return name == "index.html" ||
               name == "style.css" ||
               name == "app.js";
    }

    // ==================================================
    // Web update - PUT
    // ==================================================

    static void handleWebUpdate()
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

        String name = server.arg("name");

        if (!isAllowedWebFile(name))
        {

            sendJson(
                400,
                "{\"error\":\"File not allowed\"}");

            return;
        }

        if (!server.hasArg("plain"))
        {

            sendJson(
                400,
                "{\"error\":\"Missing file content\"}");

            return;
        }

        const String &content =
            server.arg("plain");

        size_t length = content.length();

        if (
            length == 0 ||
            length > WebUpdateConfig::MAX_FILE_SIZE)
        {

            sendJson(
                413,
                "{\"error\":\"Invalid file size\"}");

            return;
        }

        if (!LittleFS.exists("/www"))
        {

            if (!LittleFS.mkdir("/www"))
            {

                sendJson(
                    500,
                    "{\"error\":\"Cannot create web directory\"}");

                return;
            }
        }

        String path = "/www/" + name;

        String temporaryPath = path + ".tmp";

        if (LittleFS.exists(temporaryPath))
        {
            LittleFS.remove(temporaryPath);
        }

        File file = LittleFS.open(
            temporaryPath,
            "w");

        if (!file)
        {

            sendJson(
                500,
                "{\"error\":\"Cannot open temporary file\"}");

            return;
        }

        size_t written = file.write(
            reinterpret_cast<const uint8_t *>(
                content.c_str()),
            length);

        file.flush();
        file.close();

        if (written != length)
        {

            LittleFS.remove(temporaryPath);

            sendJson(
                500,
                "{\"error\":\"Incomplete write\"}");

            return;
        }

        File verification = LittleFS.open(
            temporaryPath,
            "r");

        if (!verification)
        {

            LittleFS.remove(temporaryPath);

            sendJson(
                500,
                "{\"error\":\"Verification failed\"}");

            return;
        }

        bool verified =
            verification.size() == length;

        for (
            size_t i = 0;
            verified && i < length;
            i++)
        {

            int value = verification.read();

            if (
                value !=
                static_cast<uint8_t>(content[i]))
            {
                verified = false;
            }

            if (i % 512 == 0)
            {
                yield();
            }
        }

        verification.close();

        if (!verified)
        {

            LittleFS.remove(temporaryPath);

            sendJson(
                500,
                "{\"error\":\"File content mismatch\"}");

            return;
        }

        if (!LittleFS.rename(
                temporaryPath,
                path))
        {

            LittleFS.remove(temporaryPath);

            sendJson(
                500,
                "{\"error\":\"Cannot replace file\"}");

            return;
        }

        Serial.print("Web file updated: ");
        Serial.print(path);
        Serial.print(" (");
        Serial.print(length);
        Serial.println(" bytes)");

        String json =
            "{\"ok\":true,\"file\":\"";

        json += name;
        json += "\",\"size\":";
        json += String(length);
        json += "}";

        sendJson(200, json);
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

        server.on(
            "/api/status",
            HTTP_GET,
            handleStatus);

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

        server.onNotFound(handleNotFound);
    }

    // ==================================================
    // Server update
    // ==================================================

    void update()
    {

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
