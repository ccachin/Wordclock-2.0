
#include <Arduino.h>

#include "leds.h"
#include "time_manager.h"
#include "status_manager.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "storage.h"
#include "theme_manager.h"
#include "render_manager.h"

using StatusManager::State;

static bool wasConnected = false;

// ======================================================
// Setup
// ======================================================

void setup()
{

    Serial.begin(115200);

    Serial.println();
    Serial.println("Wordclock 3.0");
    Serial.println("Starting...");

    // Hardware

    Leds::begin();

    // System managers

    TimeManager::begin();
    StatusManager::begin();

    ThemeManager::begin();
    RenderManager::begin();

    // Connectivity and storage

    WifiManager::begin();
    Storage::begin();
    WebServer::begin();

    // Initial status

    StatusManager::setState(
        State::WifiConnecting);
}

// ======================================================
// Main loop
// ======================================================

void loop()
{

    // Connectivity

    WifiManager::update();
    WebServer::update();

    bool connected =
        WifiManager::isConnected();

    bool timeValid =
        TimeManager::isValid();

    // Start NTP after Wi-Fi connection

    if (connected && !wasConnected)
    {

        Serial.println(
            "Starting NTP synchronization");

        TimeManager::startNtp();

        StatusManager::setState(
            State::Connected);
    }

    wasConnected = connected;

    // Update system status

    if (WifiManager::isPortalActive())
    {

        StatusManager::setState(
            State::WifiSetupRequired);
    }
    else if (!connected)
    {

        StatusManager::setState(
            timeValid
                ? State::Ready
                : State::WifiConnecting);
    }
    else if (
        StatusManager::getState() !=
        State::Connected)
    {

        StatusManager::setState(
            timeValid
                ? State::Ready
                : State::TimeSyncPending);
    }

    // Update status animations

    StatusManager::update();

    // Render LEDs

    RenderManager::update();

    delay(5);
}
