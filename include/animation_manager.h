
#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "front_effects.h"

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

    const FrontEffects::Settings& frontSettings();
    bool setFrontSettings(const FrontEffects::Settings& value);
    uint32_t frontRevision();
    bool startFrontEffect(FrontEffects::Kind kind, uint32_t now);
    void cancelFrontEffect();
    bool frontEffectActive(uint32_t now);
    void applyFrontEffect(uint32_t now, const CRGB& color);
    CRGB hourlyPalette(const CRGB& base, uint8_t hour);

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
