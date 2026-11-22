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
    constexpr uint32_t PORTAL_RETRY = 15000;

    static bool wasConnected = false;
    static bool everConnected = false;
    static bool needsSetup = false;
    static bool portalAttempted = false;
    static bool manualPortal = false;
    static uint32_t disconnectedSince = 0;
    static uint32_t lastPortalAttempt = 0;

    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    bool isPortalActive() { return manager.getConfigPortalActive(); }
    String getIPAddress() { return WiFi.localIP().toString(); }

    static void openPortal(bool manual)
    {
        if (isPortalActive()) return;
        portalAttempted = true;
        manualPortal = manual;
        Serial.println("Starting Wi-Fi setup portal");
        manager.startConfigPortal(AP_NAME, AP_PASSWORD);
        lastPortalAttempt = millis();
        if (!isPortalActive())
        {
            manualPortal = false;
            Serial.println("Wi-Fi setup portal closed or failed; retry delayed");
        }
    }

    void begin()
    {
        WiFi.mode(WIFI_STA);
        WiFi.hostname("wordclock");
        WiFi.setAutoReconnect(true);
        manager.setConfigPortalBlocking(false);
        needsSetup = !manager.getWiFiIsSaved();
        disconnectedSince = millis();
        if (!needsSetup) WiFi.begin();
        // Open the portal in update(), after the other services initialize.
    }

    void startConfigPortal()
    {
        openPortal(true);
    }

    void update()
    {
        manager.process();
        const bool connected = isConnected();
        if (connected)
        {
            if (!wasConnected)
            {
                Serial.println("Wi-Fi connected");
                Serial.println(getIPAddress());
                everConnected = true;
                needsSetup = false;
            }
            if (isPortalActive() && !manualPortal)
                manager.stopConfigPortal();
            if (!isPortalActive())
            {
                manualPortal = false;
                portalAttempted = false;
            }
        }
        else
        {
            if (wasConnected)
            {
                disconnectedSince = millis();
                portalAttempted = false;
                Serial.println("Wi-Fi disconnected");
            }
            const uint32_t now = millis();
            const uint32_t timeout = everConnected ? RECONNECT_TIMEOUT : STARTUP_TIMEOUT;
            if (!isPortalActive() &&
                (needsSetup || now - disconnectedSince >= timeout) &&
                (!portalAttempted || now - lastPortalAttempt >= PORTAL_RETRY))
            {
                openPortal(false);
            }
        }
        wasConnected = connected;
    }
}
