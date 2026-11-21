#include <Arduino.h>
#include "config.h"
#include "leds.h"
#include "wordclock.h"
#include "time_manager.h"

int lastMinute = -1;
int lastSecond = -1;

void setup()
{

  Serial.begin(115200);

  Serial.println();
  Serial.println("Wordclock 3.0");
  Serial.println("Time manager test");

  Leds::begin();
  TimeManager::begin();

  // Temporary test time
  if (!TimeManager::setTime(
          2026, 11, 21, 10, 58, 50))
  {
    Serial.println("Time initialization failed");
  }
}

void loop()
{

  TimeManager::ClockTime now;

  if (!TimeManager::getTime(now))
  {
    delay(10);
    return;
  }

  // Serial output every second
  if (now.second != lastSecond)
  {

    lastSecond = now.second;

    Serial.printf(
        "%02u:%02u:%02u\n",
        now.hour,
        now.minute,
        now.second);
  }

  // Update the display when the minute changes
  if (now.minute != lastMinute)
  {

    lastMinute = now.minute;

    Wordclock::displayTime(
        now.hour,
        now.minute,
        CRGB::Green);

    Leds::show();
  }

  delay(10);
}