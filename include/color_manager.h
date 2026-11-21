
#pragma once

#include <Arduino.h>
#include <FastLED.h>

namespace ColorManager
{

    enum class Harmony : uint8_t
    {

        Manual,
        Identical,
        Complementary,
        AnalogousPlus,
        AnalogousMinus,
        Triadic
    };

    struct HSV
    {

        float hue;
        float saturation;
        float value;
    };

    HSV rgbToHsv(const CRGB &color);

    CRGB hsvToRgb(const HSV &color);

    CRGB rotateHue(
        const CRGB &color,
        float degrees);

    CRGB setSaturation(
        const CRGB &color,
        float saturation);

    CRGB setValue(
        const CRGB &color,
        float value);

    CRGB interpolate(
        const CRGB &from,
        const CRGB &to,
        float progress);

    CRGB resolveRearColor(
        const CRGB &front,
        const CRGB &rear,
        Harmony harmony);

    const char *harmonyToString(
        Harmony harmony);

    bool harmonyFromString(
        const String &name,
        Harmony &harmony);

    void runDiagnostics();

}
