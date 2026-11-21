#pragma once

#include <Arduino.h>
#include <FastLED.h>

namespace Wordclock
{

    void displayTime(
        uint8_t hour,
        uint8_t minute,
        CRGB color);

}