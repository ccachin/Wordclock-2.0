#include <Arduino.h>
#include "config.h"
#include "leds.h"
#include "wordclock.h"

struct TestTime
{
  uint8_t hour;
  uint8_t minute;
};

constexpr TestTime testTimes[] = {
    {10, 0},
    {10, 5},
    {10, 15},
    {10, 28},
    {10, 30},
    {10, 35},
    {10, 45},
    {10, 59},
    {12, 0},
    {0, 0},
    {1, 0},
    {23, 58}};

constexpr size_t TEST_COUNT =
    sizeof(testTimes) / sizeof(testTimes[0]);

size_t testIndex = 0;

void setup()
{

  Serial.begin(115200);

  Serial.println();
  Serial.println("Wordclock 3.0");
  Serial.println("Time display test");

  Leds::begin();
}

void loop()
{

  const TestTime &time = testTimes[testIndex];

  Serial.printf(
      "Displaying %02u:%02u\n",
      time.hour,
      time.minute);

  Wordclock::displayTime(
      time.hour,
      time.minute,
      CRGB::Green);

  Leds::show();

  testIndex = (testIndex + 1) % TEST_COUNT;

  delay(5000);
}