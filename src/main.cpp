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

bool wasConnected = false;

void setup()
{

  Serial.begin(115200);

  Serial.println();
  Serial.println("Wordclock 3.0");
  Serial.println("Starting...");

  Leds::begin();

  TimeManager::begin();
  StatusManager::begin();

  ThemeManager::begin();
  RenderManager::begin();

  WifiManager::begin();
  Storage::begin();
  WebServer::begin();

  StatusManager::setState(
      State::WifiConnecting);
}

void loop()
{

  WifiManager::update();
  WebServer::update();

  bool connected = WifiManager::isConnected();
  bool timeValid = TimeManager::isValid();

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

  RenderManager::update();
  StatusManager::update();

  delay(5);
}