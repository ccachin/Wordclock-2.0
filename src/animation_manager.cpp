
#include "animation_manager.h"

#include "color_manager.h"

namespace AnimationManager
{

    // ==================================================
    // Configuration
    // ==================================================

    constexpr uint32_t MIN_PERIOD_MS = 1000;

    constexpr uint32_t MAX_PERIOD_MS = 30000;

    constexpr uint8_t MIN_BREATHING_LEVEL = 40;

    constexpr uint8_t MIN_CHASE_LEVEL = 12;

    // ==================================================
    // Animation state
    // ==================================================

    bool isAnimated(
        const Animation &animation)
    {

        return animation.effect != Effect::Solid;
    }

    // ==================================================
    // Speed conversion
    // ==================================================

    uint32_t getPeriod(
        const Animation &animation)
    {

        uint8_t speed = animation.speed;

        if (speed > 100)
        {

            speed = 100;
        }

        return MAX_PERIOD_MS -

               ((MAX_PERIOD_MS - MIN_PERIOD_MS) * speed / 100);
    }

    // ==================================================
    // Internal helpers
    // ==================================================

    static uint8_t getPhase(
        uint32_t elapsedMs,
        uint32_t period)
    {

        return static_cast<uint8_t>(

            (
                (elapsedMs % period) * 256UL) /
            period

        );
    }

    static uint8_t normalizeWave(
        uint8_t wave,
        uint8_t minimum)
    {

        return minimum +

               (static_cast<uint16_t>(wave) * (255 - minimum)) / 255;
    }

    // ==================================================
    // Solid effect
    // ==================================================

    static CRGB solid(
        const CRGB &baseColor)
    {

        return baseColor;
    }

    // ==================================================
    // Breathing effect
    // ==================================================

    static CRGB breathing(
        const CRGB &baseColor,
        uint8_t phase)
    {

        uint8_t wave = sin8(

            static_cast<uint8_t>(
                phase + 64));

        uint8_t level = normalizeWave(

            wave,

            MIN_BREATHING_LEVEL

        );

        CRGB color = baseColor;

        color.nscale8_video(level);

        return color;
    }

    // ==================================================
    // Color cycle effect
    // ==================================================

    static CRGB colorCycle(
        const CRGB &baseColor,
        uint32_t elapsedMs,
        uint32_t period)
    {

        float degrees =

            static_cast<float>(
                elapsedMs % period)

            * 360.0f / period;

        return ColorManager::rotateHue(

            baseColor,

            degrees);
    }

    // ==================================================
    // Rainbow effect
    // ==================================================

    static CRGB rainbow(
        const CRGB &baseColor,
        uint8_t phase,
        uint16_t pixelIndex,
        uint16_t pixelCount)
    {

        uint8_t offset =

            static_cast<uint8_t>(

                (
                    static_cast<uint32_t>(
                        pixelIndex) *
                    256UL) /
                pixelCount

            );

        uint8_t hue =

            static_cast<uint8_t>(
                offset - phase);

        uint8_t value = baseColor.r;

        if (baseColor.g > value)
        {

            value = baseColor.g;
        }

        if (baseColor.b > value)
        {

            value = baseColor.b;
        }

        CRGB color;

        hsv2rgb_rainbow(

            CHSV(
                hue,
                255,
                value),

            color);

        return color;
    }

    // ==================================================
    // Chase effect
    // ==================================================

    static CRGB chase(
        const CRGB &baseColor,
        uint8_t phase,
        uint16_t pixelIndex,
        uint16_t pixelCount)
    {

        uint8_t offset =

            static_cast<uint8_t>(

                (
                    static_cast<uint32_t>(
                        pixelIndex) *
                    256UL) /
                pixelCount

            );

        uint8_t wave = sin8(

            static_cast<uint8_t>(
                offset - phase + 64));

        // Narrow the illuminated section.

        wave = scale8(wave, wave);

        wave = scale8(wave, wave);

        uint8_t level = normalizeWave(

            wave,

            MIN_CHASE_LEVEL);

        CRGB color = baseColor;

        color.nscale8_video(level);

        return color;
    }

    // ==================================================
    // Main animation engine
    // ==================================================

    CRGB getColor(
        const CRGB &baseColor,
        const Animation &animation,
        uint32_t elapsedMs,
        uint16_t pixelIndex,
        uint16_t pixelCount)
    {

        if (pixelCount == 0)
        {

            pixelCount = 1;
        }

        pixelIndex %= pixelCount;

        if (animation.effect == Effect::Solid)
        {

            return baseColor;
        }

        uint32_t period = getPeriod(

            animation);

        uint8_t phase = getPhase(

            elapsedMs,

            period);

        switch (animation.effect)
        {

        case Effect::Breathing:

            return breathing(
                baseColor,
                phase);

        case Effect::ColorCycle:

            return colorCycle(
                baseColor,
                elapsedMs,
                period);

        case Effect::Rainbow:

            return rainbow(
                baseColor,
                phase,
                pixelIndex,
                pixelCount);

        case Effect::Chase:

            return chase(
                baseColor,
                phase,
                pixelIndex,
                pixelCount);

        case Effect::Solid:
        default:

            return solid(baseColor);
        }
    }

    // ==================================================
    // Serialization
    // ==================================================

    const char *effectToString(
        Effect effect)
    {

        switch (effect)
        {

        case Effect::Breathing:

            return "breathing";

        case Effect::ColorCycle:

            return "color-cycle";

        case Effect::Rainbow:

            return "rainbow";

        case Effect::Chase:

            return "chase";

        case Effect::Solid:
        default:

            return "solid";
        }
    }

    bool effectFromString(
        const String &name,
        Effect &effect)
    {

        if (name == "solid")
        {

            effect = Effect::Solid;
        }
        else if (name == "breathing")
        {

            effect = Effect::Breathing;
        }
        else if (name == "color-cycle")
        {

            effect = Effect::ColorCycle;
        }
        else if (name == "rainbow")
        {

            effect = Effect::Rainbow;
        }
        else if (name == "chase")
        {

            effect = Effect::Chase;
        }
        else
        {

            return false;
        }

        return true;
    }

}
