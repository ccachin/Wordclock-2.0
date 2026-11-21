#include <Arduino.h>
#include "leds.h"
#include "wordclock.h"
#include "time_manager.h"
#include "status_manager.h"
#include "wifi_manager.h"
#include "web_server.h"

using StatusManager::State;

bool wasConnected = false;

int lastHour = -1;
int lastMinute = -1;

void setup()
{

  Serial.begin(115200);

  Serial.println();
  Serial.println("Wordclock 3.0");
  Serial.println("Starting...");

  Leds::begin();
  TimeManager::begin();
  StatusManager::begin();
  WifiManager::begin();
  WebServer::begin();

  StatusManager::setState(State::WifiConnecting);
}

void loop()
{

  WifiManager::update();
  WebServer::update();

  bool connected = WifiManager::isConnected();
  bool timeValid = TimeManager::isValid();

  // Start NTP after connection
  if (connected && !wasConnected)
  {

    Serial.println("Starting NTP synchronization");

    TimeManager::startNtp();

    StatusManager::setState(State::Connected);
  }

  wasConnected = connected;

  // Update status indicators
  if (WifiManager::isPortalActive())
  {

    StatusManager::setState(
        State::WifiSetupRequired);
  }
  else if (!connected && !timeValid)
  {

    StatusManager::setState(
        State::WifiConnecting);
  }
  else if (
      connected &&
      StatusManager::getState() != State::Connected)
  {

    StatusManager::setState(
        timeValid
            ? State::Ready
            : State::TimeSyncPending);
  }

  // Update time display
  TimeManager::ClockTime now;

  if (TimeManager::getTime(now))
  {

    if (now.hour != lastHour ||
        now.minute != lastMinute)
    {

      lastHour = now.hour;
      lastMinute = now.minute;

      Wordclock::displayTime(
          now.hour,
          now.minute,
          CRGB::Green);

      Leds::show();

      Serial.printf(
          "Time: %02u:%02u\n",
          now.hour,
          now.minute);
    }
  }

  StatusManager::update();

  delay(5);
}