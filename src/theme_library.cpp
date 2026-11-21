
#include "theme_library.h"

#include <ArduinoJson.h>
#include <FastLED.h>
#include <ctype.h>
#include <stdlib.h>

#include "theme_manager.h"
#include "color_manager.h"

namespace ThemeLibrary
{

    static const char FACTORY_THEMES[] = R"json(
    [
        {
            "version": 1,
            "id": "warm-white",
            "name": "Warm White",
            "author": "Wordclock",
            "front": "#FFE1B4",
            "rear": "#B87838"
        },
        {
            "version": 1,
            "id": "ocean",
            "name": "Ocean",
            "author": "Wordclock",
            "front": "#A9EBFF",
            "rear": "#267DD9"
        },
        {
            "version": 1,
            "id": "sunset",
            "name": "Sunset",
            "author": "Wordclock",
            "front": "#FFB568",
            "rear": "#A63372"
        },
        {
            "version": 1,
            "id": "forest",
            "name": "Forest",
            "author": "Wordclock",
            "front": "#C9E8A5",
            "rear": "#296B48"
        },
        {
            "version": 1,
            "id": "aurora",
            "name": "Aurora",
            "author": "Wordclock",
            "front": "#79F2D0",
            "rear": "#7848D8"
        }
    ]
    )json";

    static bool parseColor(
        const char *value,
        CRGB &color)
    {

        if (
            value == nullptr ||
            strlen(value) != 7 ||
            value[0] != '#')
        {

            return false;
        }

        for (size_t i = 1; i < 7; i++)
        {

            if (!isxdigit(
                    static_cast<unsigned char>(
                        value[i])))
            {

                return false;
            }
        }

        uint32_t rgb =
            strtoul(value + 1, nullptr, 16);

        color = CRGB(
            (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF,
            rgb & 0xFF);

        return true;
    }

    String getCatalog()
    {

        return String(FACTORY_THEMES);
    }

    bool applyTheme(const String &id)
    {

        DynamicJsonDocument doc(2048);

        DeserializationError error =
            deserializeJson(
                doc,
                FACTORY_THEMES);

        if (error)
        {

            Serial.print(
                "Theme catalog error: ");

            Serial.println(error.c_str());

            return false;
        }

        JsonArray themes =
            doc.as<JsonArray>();

        for (JsonObject entry : themes)
        {

            const char *themeId =
                entry["id"].as<const char *>();

            if (
                themeId == nullptr ||
                id != themeId)
            {

                continue;
            }

            ThemeManager::Theme theme =
                ThemeManager::getTheme();

            CRGB front;
            CRGB rear;

            if (!parseColor(
                    entry["front"].as<const char *>(),
                    front))
            {

                return false;
            }

            if (!parseColor(
                    entry["rear"].as<const char *>(),
                    rear))
            {

                return false;
            }

            theme.frontColor = front;

            theme.rearColor = rear;

            theme.harmony =
                ColorManager::Harmony::Manual;

            // Preserve global brightness.

            ThemeManager::setTheme(theme);

            Serial.print("Theme applied: ");

            Serial.println(id);

            return true;
        }

        return false;
    }

}
