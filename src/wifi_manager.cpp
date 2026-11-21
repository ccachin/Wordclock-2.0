#include "wifi_manager.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

namespace WifiManager
{

    static WiFiManager manager;

    constexpr char AP_NAME[] = "Wordclock-Setup";
    constexpr char AP_PASSWORD[] = "Wordclock123";

    constexpr uint32_t STARTUP_TIMEOUT = 30000;
    constexpr uint32_t RECONNECT_TIMEOUT = 300000;

    static bool wasConnected = false;
    static bool everConnected = false;
    static uint32_t disconnectedSince = 0;

    void begin()
    {

        WiFi.mode(WIFI_STA);
        WiFi.hostname("wordclock");
        WiFi.setAutoReconnect(true);

        manager.setConfigPortalBlocking(false);

        disconnectedSince = millis();

        WiFi.begin();
    }

    bool isConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    bool isPortalActive()
    {
        return manager.getConfigPortalActive();
    }

    void startConfigPortal()
    {

        if (!isPortalActive())
        {

            Serial.println("Starting Wi-Fi setup portal");

            manager.startConfigPortal(
                AP_NAME,
                AP_PASSWORD);
        }
    }

    String getIPAddress()
    {
        return WiFi.localIP().toString();
    }

    void update()
    {

        manager.process();

        bool connected = isConnected();

        if (connected)
        {

            if (!wasConnected)
            {

                Serial.println("Wi-Fi connected");
                Serial.println(getIPAddress());

                everConnected = true;
            }

            if (isPortalActive())
            {
                manager.stopConfigPortal();
            }
        }
        else
        {

            if (wasConnected)
            {
                disconnectedSince = millis();
                Serial.println("Wi-Fi disconnected");
            }

            uint32_t timeout = everConnected
                                   ? RECONNECT_TIMEOUT
                                   : STARTUP_TIMEOUT;

            if (!isPortalActive() &&
                millis() - disconnectedSince >= timeout)
            {

                startConfigPortal();
            }
        }

        wasConnected = connected;
    }

}