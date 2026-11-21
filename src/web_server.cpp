#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>

#include "web_server.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "storage.h"

namespace WebServer
{

    static ESP8266WebServer server(80);

    static bool running = false;
    static bool mdnsRunning = false;

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

        server.sendHeader("Cache-Control", "no-store");
        server.streamFile(file, contentType);

        file.close();
    }

    static void handleRoot()
    {

        serveFile(
            "/www/index.html",
            "text/html; charset=utf-8");
    }

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
            static_cast<unsigned long>(millis() / 1000),
            static_cast<unsigned long>(ESP.getFreeHeap()));

        server.sendHeader("Cache-Control", "no-store");

        server.send(
            200,
            "application/json",
            json);
    }

    static void handleNotFound()
    {

        server.send(
            404,
            "application/json",
            "{\"error\":\"Not found\"}");
    }

    void begin()
    {

        server.on("/", HTTP_GET, handleRoot);
        server.on("/style.css", HTTP_GET, []()
                  { serveFile(
                        "/www/style.css",
                        "text/css"); });

        server.on("/app.js", HTTP_GET, []()
                  { serveFile(
                        "/www/app.js",
                        "application/javascript"); });
        server.on(
            "/api/status",
            HTTP_GET,
            handleStatus);

        server.onNotFound(handleNotFound);
    }

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

                MDNS.addService("http", "tcp", 80);
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