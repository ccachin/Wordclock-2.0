#pragma once

#include <Arduino.h>
#include <FastLED.h>

namespace ThemeManager
{

    struct Theme
    {
        CRGB frontColor;
        CRGB rearColor;
        uint8_t brightness;
    };

    void begin();

    const Theme &getTheme();

    void setFrontColor(CRGB color);
    void setRearColor(CRGB color);
    void setBrightness(uint8_t brightness);

    void setTheme(const Theme &theme);

    bool hasChanged();
    void markRendered();

}