
#pragma once

#include <Arduino.h>
#include <FastLED.h>

namespace AnimationManager
{

    enum class Effect : uint8_t
    {

        Solid,
        Breathing,
        ColorCycle,
        Rainbow,
        Chase
    };

    struct Animation
    {

        Effect effect = Effect::Solid;

        uint8_t speed = 50;
    };

    bool isAnimated(
        const Animation &animation);

    uint32_t getPeriod(
        const Animation &animation);

    CRGB getColor(
        const CRGB &baseColor,
        const Animation &animation,
        uint32_t elapsedMs,
        uint16_t pixelIndex = 0,
        uint16_t pixelCount = 1);

    const char *effectToString(
        Effect effect);

    bool effectFromString(
        const String &name,
        Effect &effect);

}
