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

    // Diagnostic: use the unmodified native WiFiManager interface.

    // Keep the captive portal available briefly after a verified save.
    static bool savePending = false;
    static bool saveSucceeded = false;
    static bool saveFailed = false;
    static bool finishRequested = false;
    static uint32_t successAt = 0;
    static uint32_t finishAt = 0;
    constexpr uint32_t HANDOFF_TIMEOUT = 60000;

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
        savePending = false;
        saveSucceeded = false;
        saveFailed = false;
        finishRequested = false;
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
        manager.setTitle("Wordclock");
        const char* menu[] = {"wifi"};
        manager.setMenu(menu, 1);
        manager.setShowPassword(false);
        manager.setDisableConfigPortal(false);
        manager.setBreakAfterConfig(false);
        manager.setConnectTimeout(20);
        manager.setSaveConnectTimeout(20);
        manager.setConnectRetries(1);
        manager.setPreSaveConfigCallback([]() {
            savePending = true;
            saveSucceeded = false;
            saveFailed = false;
            finishRequested = false;
        });
        manager.setSaveConfigCallback([]() {
            if (savePending && isConnected()) {
                saveSucceeded = true;
                successAt = millis();
            }
        });
        manager.setWebServerCallback([]() {
            manager.server->on("/wc-handoff", HTTP_GET, []() {
                String body = "{\"ready\":";
                const bool ready = saveSucceeded && isConnected();
                body += ready ? "true" : "false";
                body += ",\"failed\":";
                body += saveFailed ? "true" : "false";
                body += ",\"ip\":\"";
                if (ready) body += getIPAddress();
                body += "\"}";
                manager.server->sendHeader("Cache-Control", "no-store");
                manager.server->send(200, "application/json", body);
            });
            manager.server->on("/wc-finish", HTTP_POST, []() {
                if (!saveSucceeded || !isConnected()) {
                    manager.server->send(409, "text/plain", "Not connected");
                    return;
                }
                finishRequested = true;
                finishAt = millis();
                manager.server->send(200, "text/plain", "OK");
            });
        });
        needsSetup = !manager.getWiFiIsSaved();
        disconnectedSince = millis();
        if (!needsSetup) WiFi.begin();
        // Open the portal in update(), after the other services initialize.
    }

    void startConfigPortal()
    {
        openPortal(true);
    }

    void forgetNetwork()
    {
        manager.resetSettings();
        ESP.restart();
    }

    void update()
    {
        manager.process();
        if (savePending) {
            // process() handles a submitted connection synchronously.
            saveFailed = !saveSucceeded;
            savePending = false;
        }
        if (saveSucceeded && !isConnected()) {
            saveSucceeded = false;
            finishRequested = false;
        }
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
            if (isPortalActive()) {
                const uint32_t now = millis();
                if (saveSucceeded) {
                    if ((finishRequested && now - finishAt >= 1200) ||
                        now - successAt >= HANDOFF_TIMEOUT)
                        manager.stopConfigPortal();
                } else if (!manualPortal && !savePending && !saveFailed) {
                    manager.stopConfigPortal();
                }
            }
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
