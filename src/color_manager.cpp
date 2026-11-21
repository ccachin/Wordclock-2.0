
#include "color_manager.h"

#include <math.h>

namespace ColorManager
{

    // ==================================================
    // Internal helpers
    // ==================================================

    static float clamp01(float value)
    {

        if (value < 0.0f)
        {
            return 0.0f;
        }

        if (value > 1.0f)
        {
            return 1.0f;
        }

        return value;
    }

    static float normalizeHue(float hue)
    {

        hue = fmodf(hue, 360.0f);

        if (hue < 0.0f)
        {
            hue += 360.0f;
        }

        return hue;
    }

    static uint8_t toByte(float value)
    {

        value = clamp01(value);

        return static_cast<uint8_t>(
            roundf(value * 255.0f));
    }

    // ==================================================
    // RGB -> HSV
    // ==================================================

    HSV rgbToHsv(const CRGB &color)
    {

        float r = color.r / 255.0f;
        float g = color.g / 255.0f;
        float b = color.b / 255.0f;

        float maximum = fmaxf(
            r,
            fmaxf(g, b));

        float minimum = fminf(
            r,
            fminf(g, b));

        float delta = maximum - minimum;

        HSV result;

        result.hue = 0.0f;

        result.value = maximum;

        result.saturation =
            maximum > 0.0f
                ? delta / maximum
                : 0.0f;

        if (delta > 0.0f)
        {

            if (maximum == r)
            {

                result.hue = 60.0f *
                             fmodf(
                                 (g - b) / delta,
                                 6.0f);
            }
            else if (maximum == g)
            {

                result.hue = 60.0f *
                             ((b - r) / delta +
                              2.0f);
            }
            else
            {

                result.hue = 60.0f *
                             ((r - g) / delta +
                              4.0f);
            }
        }

        result.hue =
            normalizeHue(result.hue);

        return result;
    }

    // ==================================================
    // HSV -> RGB
    // ==================================================

    CRGB hsvToRgb(const HSV &color)
    {

        float h = normalizeHue(color.hue);

        float s = clamp01(
            color.saturation);

        float v = clamp01(color.value);

        float c = v * s;

        float x = c *
                  (1.0f -
                   fabsf(
                       fmodf(h / 60.0f, 2.0f) - 1.0f));

        float m = v - c;

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        if (h < 60.0f)
        {

            r = c;
            g = x;
        }
        else if (h < 120.0f)
        {

            r = x;
            g = c;
        }
        else if (h < 180.0f)
        {

            g = c;
            b = x;
        }
        else if (h < 240.0f)
        {

            g = x;
            b = c;
        }
        else if (h < 300.0f)
        {

            r = x;
            b = c;
        }
        else
        {

            r = c;
            b = x;
        }

        return CRGB(
            toByte(r + m),
            toByte(g + m),
            toByte(b + m));
    }

    // ==================================================
    // Hue rotation
    // ==================================================

    CRGB rotateHue(
        const CRGB &color,
        float degrees)
    {

        HSV hsv = rgbToHsv(color);

        hsv.hue = normalizeHue(
            hsv.hue + degrees);

        return hsvToRgb(hsv);
    }

    // ==================================================
    // Saturation
    // ==================================================

    CRGB setSaturation(
        const CRGB &color,
        float saturation)
    {

        HSV hsv = rgbToHsv(color);

        hsv.saturation =
            clamp01(saturation);

        return hsvToRgb(hsv);
    }

    // ==================================================
    // HSV value
    // ==================================================

    CRGB setValue(
        const CRGB &color,
        float value)
    {

        HSV hsv = rgbToHsv(color);

        hsv.value = clamp01(value);

        return hsvToRgb(hsv);
    }

    // ==================================================
    // Linear RGB interpolation
    // ==================================================

    CRGB interpolate(
        const CRGB &from,
        const CRGB &to,
        float progress)
    {

        progress = clamp01(progress);

        return CRGB(

            static_cast<uint8_t>(
                roundf(
                    from.r +
                    (to.r - from.r) * progress)),

            static_cast<uint8_t>(
                roundf(
                    from.g +
                    (to.g - from.g) * progress)),

            static_cast<uint8_t>(
                roundf(
                    from.b +
                    (to.b - from.b) * progress)));
    }

    // ==================================================
    // Harmony resolution
    // ==================================================

    CRGB resolveRearColor(
        const CRGB &front,
        const CRGB &rear,
        Harmony harmony)
    {

        switch (harmony)
        {

        case Harmony::Manual:

            return rear;

        case Harmony::Identical:

            return front;

        case Harmony::Complementary:

            return rotateHue(
                front,
                180.0f);

        case Harmony::AnalogousPlus:

            return rotateHue(
                front,
                30.0f);

        case Harmony::AnalogousMinus:

            return rotateHue(
                front,
                -30.0f);

        case Harmony::Triadic:

            return rotateHue(
                front,
                120.0f);

        default:

            return rear;
        }
    }

    // ==================================================
    // Harmony serialization
    // ==================================================

    const char *harmonyToString(
        Harmony harmony)
    {

        switch (harmony)
        {

        case Harmony::Identical:
            return "identical";

        case Harmony::Complementary:
            return "complementary";

        case Harmony::AnalogousPlus:
            return "analogous-plus";

        case Harmony::AnalogousMinus:
            return "analogous-minus";

        case Harmony::Triadic:
            return "triadic";

        case Harmony::Manual:
        default:
            return "manual";
        }
    }

    bool harmonyFromString(
        const String &name,
        Harmony &harmony)
    {

        if (name == "manual")
        {

            harmony = Harmony::Manual;
        }
        else if (name == "identical")
        {

            harmony = Harmony::Identical;
        }
        else if (name == "complementary")
        {

            harmony = Harmony::Complementary;
        }
        else if (name == "analogous-plus")
        {

            harmony = Harmony::AnalogousPlus;
        }
        else if (name == "analogous-minus")
        {

            harmony = Harmony::AnalogousMinus;
        }
        else if (name == "triadic")
        {

            harmony = Harmony::Triadic;
        }
        else
        {

            return false;
        }

        return true;
    }

    // ==================================================
    // Diagnostics
    // ==================================================

    static bool checkColor(
        const char *name,
        const CRGB &actual,
        const CRGB &expected)
    {

        bool success =

            abs(
                static_cast<int>(actual.r) -
                expected.r) <= 1 &&

            abs(
                static_cast<int>(actual.g) -
                expected.g) <= 1 &&

            abs(
                static_cast<int>(actual.b) -
                expected.b) <= 1;

        Serial.printf(
            "[%s] %s: #%02X%02X%02X"
            " (expected #%02X%02X%02X)\n",

            success ? "OK" : "FAIL",

            name,

            actual.r,
            actual.g,
            actual.b,

            expected.r,
            expected.g,
            expected.b);

        return success;
    }

    void runDiagnostics()
    {

        Serial.println();

        Serial.println(
            "ColorManager diagnostics");

        Serial.println(
            "========================");

        unsigned int passed = 0;
        unsigned int total = 0;

        auto test = [&](
                        const char *name,
                        const CRGB &actual,
                        const CRGB &expected)
        {
            total++;

            if (checkColor(
                    name,
                    actual,
                    expected))
            {

                passed++;
            }
        };

        const CRGB red(255, 0, 0);
        const CRGB blue(0, 0, 255);

        test(
            "Red complementary",

            resolveRearColor(
                red,
                blue,
                Harmony::Complementary),

            CRGB(0, 255, 255));

        test(
            "Red analogous +30",

            rotateHue(red, 30.0f),

            CRGB(255, 128, 0));

        test(
            "Red analogous -30",

            rotateHue(red, -30.0f),

            CRGB(255, 0, 128));

        test(
            "Red triadic",

            rotateHue(red, 120.0f),

            CRGB(0, 255, 0));

        test(
            "Manual harmony",

            resolveRearColor(
                red,
                blue,
                Harmony::Manual),

            blue);

        test(
            "Identical harmony",

            resolveRearColor(
                red,
                blue,
                Harmony::Identical),

            red);

        test(
            "Red to blue interpolation",

            interpolate(
                red,
                blue,
                0.5f),

            CRGB(128, 0, 128));

        test(
            "Zero saturation",

            setSaturation(red, 0.0f),

            CRGB::White);

        test(
            "Half value",

            setValue(red, 0.5f),

            CRGB(128, 0, 0));

        test(
            "White hue rotation",

            rotateHue(
                CRGB::White,
                180.0f),

            CRGB::White);

        test(
            "Black hue rotation",

            rotateHue(
                CRGB::Black,
                180.0f),

            CRGB::Black);

        CRGB warmWhite(
            255,
            225,
            180);

        test(
            "RGB HSV round trip",

            hsvToRgb(
                rgbToHsv(warmWhite)),

            warmWhite);

        Serial.println();

        Serial.printf(
            "ColorManager: %u/%u tests passed\n",
            passed,
            total);

        Serial.println();
    }

}
