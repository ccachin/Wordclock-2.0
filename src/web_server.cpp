#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "web_server.h"
#include "wifi_manager.h"
#include "time_manager.h"

namespace WebServer
{

    static ESP8266WebServer server(80);

    static bool running = false;
    static bool mdnsRunning = false;

    static void handleRoot()
    {

        server.send(
            200,
            "text/html",
            R"html(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport"
          content="width=device-width, initial-scale=1">
    <title>Wordclock 3.0</title>

    <style>
        body {
            background: #171819;
            color: #eeeeee;
            font-family: Arial, sans-serif;
            text-align: center;
            padding: 40px 20px;
        }

        h1 {
            font-size: 40px;
            font-weight: normal;
        }

        #clock {
            font-size: 64px;
            margin: 40px 0;
        }

        #status {
            color: #70c596;
        }
    </style>
</head>

<body>

    <h1>wordclock.</h1>

    <div id="status">Connexion...</div>
    <div id="clock">--:--</div>

    <p>Wordclock 3.0</p>

    <script>

        async function updateClock() {
            try {
                const response = await fetch(
                    '/api/status',
                    { cache: 'no-store' }
                );

                if (!response.ok) {
                    throw new Error('HTTP error');
                }

                const data = await response.json();

                document.getElementById('clock')
                    .textContent = data.time || '--:--';

                document.getElementById('status')
                    .textContent = 'Connectée';

            } catch (error) {

                document.getElementById('status')
                    .textContent = 'Connexion perdue';
            }
        }

        updateClock();

        setInterval(updateClock, 1000);

    </script>

</body>
</html>
)html");
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