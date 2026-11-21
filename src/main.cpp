#include <Arduino.h>
#include "config.h"
#include "leds.h"

void setup()
{
  Serial.begin(115200);

  Serial.println();
  Serial.println("Wordclock 3.0");
  Serial.println("LED module test");

  Leds::begin();

  // Front corners
  Leds::setFrontPixel(0, 0, CRGB::Red);
  Leds::setFrontPixel(10, 0, CRGB::Green);
  Leds::setFrontPixel(0, 10, CRGB::Blue);
  Leds::setFrontPixel(10, 10, CRGB::White);

  // Rear pixels
  Leds::setRearPixel(0, CRGB::Purple);
  Leds::setRearPixel(19, CRGB::Purple);

  Leds::show();

  Serial.println("LED test ready");
}

void loop()
{
}