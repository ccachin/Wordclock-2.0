
#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "color_manager.h"
#include "animation_manager.h"

namespace ThemeManager
{

    struct Theme
    {

        CRGB frontColor;
        CRGB rearColor;
        uint8_t brightness;

        ColorManager::Harmony harmony =
            ColorManager::Harmony::Manual;
        AnimationManager::Animation frontAnimation;
        AnimationManager::Animation rearAnimation;
    };

    void begin();

    const Theme &getTheme();

    CRGB getEffectiveRearColor();

    void setFrontColor(CRGB color);

    void setRearColor(CRGB color);

    void setBrightness(uint8_t brightness);

    void setHarmony(
        ColorManager::Harmony harmony);

    void setTheme(const Theme &theme);

    bool hasChanged();

    void markRendered();

}
