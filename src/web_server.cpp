
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

namespace WebServer
{

    static ESP8266WebServer server(80);

    static bool running = false;
    static bool mdnsRunning = false;

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
    // System status API
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

        server.sendHeader(
            "Cache-Control",
            "no-store");

        server.send(
            200,
            "application/json",
            json);
    }

    // ==================================================
    // Color conversion
    // ==================================================

    static bool parseColor(
        const String &value,
        CRGB &color)
    {

        if (value.length() != 7 ||
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

    static void handleGetTheme()
    {

        const auto &theme =
            ThemeManager::getTheme();

        char json[128];

        snprintf(
            json,
            sizeof(json),
            "{"
            "\"front\":\"#%02X%02X%02X\","
            "\"rear\":\"#%02X%02X%02X\","
            "\"brightness\":%u"
            "}",
            theme.frontColor.r,
            theme.frontColor.g,
            theme.frontColor.b,
            theme.rearColor.r,
            theme.rearColor.g,
            theme.rearColor.b,
            theme.brightness);

        server.sendHeader(
            "Cache-Control",
            "no-store");

        server.send(
            200,
            "application/json",
            json);
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

                server.send(
                    400,
                    "application/json",
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

                server.send(
                    400,
                    "application/json",
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

            if (value.length() == 0 ||
                end == value.c_str() ||
                *end != '\0' ||
                brightness < 0 ||
                brightness > 255)
            {

                server.send(
                    400,
                    "application/json",
                    "{\"error\":\"Invalid brightness\"}");

                return;
            }

            theme.brightness =
                static_cast<uint8_t>(brightness);
        }

        ThemeManager::setTheme(theme);

        handleGetTheme();
    }

    // ==================================================
    // Theme library - GET
    // ==================================================

    static void handleGetThemes()
    {

        server.sendHeader(
            "Cache-Control",
            "no-store");

        server.send(
            200,
            "application/json",
            ThemeLibrary::getCatalog());
    }

    // ==================================================
    // Apply theme - POST
    // ==================================================

    static void handleApplyTheme()
    {

        if (!server.hasArg("id"))
        {

            server.send(
                400,
                "application/json",
                "{\"error\":\"Missing theme id\"}");

            return;
        }

        String id = server.arg("id");

        if (id.length() == 0 ||
            id.length() > 64)
        {

            server.send(
                400,
                "application/json",
                "{\"error\":\"Invalid theme id\"}");

            return;
        }

        if (!ThemeLibrary::applyTheme(id))
        {

            server.send(
                404,
                "application/json",
                "{\"error\":\"Theme not found or invalid\"}");

            return;
        }

        handleGetTheme();
    }

    // ==================================================
    // Error handling
    // ==================================================

    static void handleNotFound()
    {

        server.send(
            404,
            "application/json",
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
                    "text/css");
            });

        server.on(
            "/app.js",
            HTTP_GET,
            []()
            {
                serveFile(
                    "/www/app.js",
                    "application/javascript");
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

            Serial.println(
                "Web server started");

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

                Serial.println(
                    "mDNS failed");
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
