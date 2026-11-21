#include <Arduino.h>

#include "leds.h"
#include "time_manager.h"
#include "status_manager.h"
#include "wifi_manager.h"
#include "web_server.h"
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

    Leds::begin();

    Storage::begin();

    TimeManager::begin();

    StatusManager::begin();

    ThemeManager::begin();

    SettingsManager::begin();

    ColorManager::runDiagnostics();
    CustomThemeStore::runDiagnostics();

    RenderManager::begin();

    WifiManager::begin();

    WebServer::begin();

    OTAManager::begin();

    StatusManager::setState(
        State::WifiConnecting);
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