
#include "theme_library.h"

#include <ArduinoJson.h>
#include <FastLED.h>
#include <ctype.h>
#include <stdlib.h>

#include "theme_manager.h"
#include "color_manager.h"
#include "transition_manager.h"
#include "custom_theme_store.h"

namespace ThemeLibrary
{

    static const char FACTORY_THEMES[] = R"json(
[
    {
        "version": 2,
        "id": "warm-white",
        "name": "Warm White",
        "author": "Wordclock",
        "front": "#FFE1B4",
        "rear": "#B87838",
        "frontEffect": "solid",
        "rearEffect": "solid",
        "frontSpeed": 50,
        "rearSpeed": 50
    },
    {
        "version": 2,
        "id": "ocean",
        "name": "Ocean",
        "author": "Wordclock",
        "front": "#A9EBFF",
        "rear": "#267DD9",
        "frontEffect": "solid",
        "rearEffect": "solid",
        "frontSpeed": 50,
        "rearSpeed": 50
    },
    {
        "version": 2,
        "id": "sunset",
        "name": "Sunset",
        "author": "Wordclock",
        "front": "#FFB568",
        "rear": "#A63372",
        "frontEffect": "solid",
        "rearEffect": "solid",
        "frontSpeed": 50,
        "rearSpeed": 50
    },
    {
        "version": 2,
        "id": "forest",
        "name": "Forest",
        "author": "Wordclock",
        "front": "#C9E8A5",
        "rear": "#296B48",
        "frontEffect": "solid",
        "rearEffect": "solid",
        "frontSpeed": 50,
        "rearSpeed": 50
    },
    {
        "version": 2,
        "id": "aurora",
        "name": "Aurora",
        "author": "Wordclock",
        "front": "#79F2D0",
        "rear": "#7848D8",
        "frontEffect": "solid",
        "rearEffect": "solid",
        "frontSpeed": 50,
        "rearSpeed": 50
    },
    {
        "version": 2,
        "id": "ocean-pulse",
        "name": "Ocean Pulse",
        "author": "Wordclock",
        "front": "#A9EBFF",
        "rear": "#267DD9",
        "frontEffect": "solid",
        "rearEffect": "breathing",
        "frontSpeed": 50,
        "rearSpeed": 70
    },
    {
        "version": 2,
        "id": "aurora-flow",
        "name": "Aurora Flow",
        "author": "Wordclock",
        "front": "#79F2D0",
        "rear": "#7848D8",
        "frontEffect": "solid",
        "rearEffect": "color-cycle",
        "frontSpeed": 50,
        "rearSpeed": 50
    },
    {
        "version": 2,
        "id": "rainbow",
        "name": "Rainbow",
        "author": "Wordclock",
        "front": "#FFE1B4",
        "rear": "#FFFFFF",
        "frontEffect": "solid",
        "rearEffect": "rainbow",
        "frontSpeed": 50,
        "rearSpeed": 65
    },
    {
        "version": 2,
        "id": "ember",
        "name": "Ember",
        "author": "Wordclock",
        "front": "#FFB568",
        "rear": "#C53D10",
        "frontEffect": "solid",
        "rearEffect": "chase",
        "frontSpeed": 50,
        "rearSpeed": 85
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

    static bool applyResolvedTheme(ThemeManager::Theme theme, const String& id)
    {
        const auto &old = ThemeManager::getTheme();
        theme.brightness = old.brightness;
        theme.frontBrightness = old.frontBrightness;
        theme.rearBrightness = old.rearBrightness;
        const bool different = old.frontColor != theme.frontColor ||
            old.rearColor != theme.rearColor || old.harmony != theme.harmony ||
            old.frontAnimation.effect != theme.frontAnimation.effect ||
            old.frontAnimation.speed != theme.frontAnimation.speed ||
            old.rearAnimation.effect != theme.rearAnimation.effect ||
            old.rearAnimation.speed != theme.rearAnimation.speed;
        ThemeManager::setTheme(theme);
        if (different) TransitionManager::request();
        Serial.print("Theme applied: ");
        Serial.println(id);
        return true;
    }

    bool applyTheme(const String &id)
    {

        if (id.startsWith("custom-")) {
            ThemeCodec::Entry entry;
            if (!CustomThemeStore::load(id, entry)) return false;
            return applyResolvedTheme(entry.theme, id);
        }

        DynamicJsonDocument doc(6144);

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

            theme.frontAnimation = AnimationManager::Animation();
            theme.rearAnimation = AnimationManager::Animation();
            const char* f = entry["frontEffect"] | "solid";
            const char* r = entry["rearEffect"] | "solid";
            if (!AnimationManager::effectFromString(String(f), theme.frontAnimation.effect) ||
                !AnimationManager::effectFromString(String(r), theme.rearAnimation.effect)) return false;
            int fs = entry["frontSpeed"] | 50;
            int rs = entry["rearSpeed"] | 50;
            if (fs < 0 || fs > 100 || rs < 0 || rs > 100) return false;
            theme.frontAnimation.speed = fs;
            theme.rearAnimation.speed = rs;
            return applyResolvedTheme(theme, id);
        }

        return false;
    }

}
