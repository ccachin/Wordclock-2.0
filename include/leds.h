#pragma once

#include <Arduino.h>
#include <FastLED.h>

namespace Leds
{

    void begin();
    void show();
    void clear();

    void setBrightness(uint8_t brightness);

    CRGB getPixel(uint16_t index);
    void setPixel(uint16_t index, CRGB color);
    void setFrontPixel(uint8_t x, uint8_t y, CRGB color);
    void setRearPixel(uint8_t index, CRGB color);

    void clearFront();
    void clearRear();

    void fillFront(CRGB color);
    void fillRear(CRGB color);

}