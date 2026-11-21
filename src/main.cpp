#include <Arduino.h>
#include "leds.h"
#include "wordclock.h"
#include "status_manager.h"

using StatusManager::State;

uint32_t startTime = 0;

void setup()
{

  Serial.begin(115200);

  Leds::begin();
  StatusManager::begin();

  Wordclock::displayTime(10, 28, CRGB::Green);
  Leds::show();

  startTime = millis();
}

void loop()
{

  uint32_t elapsed = (millis() - startTime) % 30000UL;

  if (elapsed < 3000)
  {

    StatusManager::setState(State::Starting);
  }
  else if (elapsed < 9000)
  {

    StatusManager::setState(State::WifiConnecting);
  }
  else if (elapsed < 15000)
  {

    StatusManager::setState(State::WifiSetupRequired);
  }
  else if (elapsed < 21000)
  {

    StatusManager::setState(State::TimeSyncPending);
  }
  else if (elapsed < 23000)
  {

    StatusManager::setState(State::Connected);
  }
  else
  {

    StatusManager::setState(State::Ready);
  }

  StatusManager::update();

  delay(5);
}