#include <Arduino.h>

#include "leds.h"
#include "time_manager.h"
#include "status_manager.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "web_slot_manager.h"
#include "storage.h"
#include "theme_manager.h"
#include "render_manager.h"
#include "settings_manager.h"
#include "color_manager.h"
#include "ota_manager.h"
#include "custom_theme_store.h"

using StatusManager::State;

static bool wasConnected = false;

void setup()
{

    Serial.begin(115200);

    Serial.println();

    Serial.println("Wordclock 3.0");

    Serial.println("Starting...");

    Serial.print("Reset reason: ");
    Serial.println(ESP.getResetReason());
    Serial.print("Reset details: ");
    Serial.println(ESP.getResetInfo());
    Serial.printf("Flash configured: %lu, actual: %lu bytes\n",
                  (unsigned long)ESP.getFlashChipSize(),
                  (unsigned long)ESP.getFlashChipRealSize());
    Serial.printf("Free heap at startup: %lu bytes\n", (unsigned long)ESP.getFreeHeap());

    Serial.println("Boot: LEDs");
    Leds::begin();

    Serial.println("Boot: storage");
    Storage::begin();

    // Complete an interrupted or requested reset before loading any user data.
    if (Storage::factoryResetPending()) {
        Serial.println("Factory reset: recovery started");
        while (!Storage::clearFactoryUserData() || !WifiManager::clearSavedNetwork() || !Storage::finishFactoryReset()) {
            Serial.println("Factory reset incomplete; retrying in 5 seconds");
            delay(5000);
        }
        Serial.println("Factory reset completed");
        ESP.restart();
        return;
    }

    Serial.println("Boot: managers");
    TimeManager::begin();

    StatusManager::begin();

    ThemeManager::begin();

    SettingsManager::begin();

    ColorManager::runDiagnostics();
    CustomThemeStore::runDiagnostics();

    RenderManager::begin();

    Serial.println("Boot: Wi-Fi");
    WifiManager::begin();

    Serial.println("Boot: web server");
    WebSlotManager::begin();
    WebServer::begin();

    Serial.println("Boot: OTA");
    OTAManager::begin();

    StatusManager::setState(
        State::WifiConnecting);

    Serial.printf("Boot: setup complete; free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
}

void loop()
{

    WifiManager::update();

    OTAManager::update();

    if (OTAManager::isUpdating())
    {

        delay(5);

        return;
    }

    WebServer::update();

    bool connected =

        WifiManager::isConnected();

    bool timeValid =

        TimeManager::isValid();

    if (connected && !wasConnected)
    {

        Serial.println(
            "Starting NTP synchronization");

        TimeManager::startNtp();

        StatusManager::setState(
            State::Connected);
    }

    wasConnected = connected;

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

    StatusManager::update();

    RenderManager::update();

    SettingsManager::update();

    delay(5);
}