
#include "settings_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ctype.h>
#include <stdlib.h>

#include "storage.h"
#include "theme_manager.h"

namespace SettingsManager
{

    constexpr char CONFIG_DIR[] =
        "/config";

    constexpr char SETTINGS_PATH[] =
        "/config/settings.json";

    constexpr char TEMP_PATH[] =
        "/config/settings.tmp";

    constexpr char BACKUP_PATH[] =
        "/config/settings.bak";

    constexpr uint32_t SAVE_DELAY = 3000;
    constexpr uint32_t RETRY_DELAY = 10000;

    static bool dirty = false;
    static bool initialized = false;

    static uint32_t lastChangeAt = 0;
    static uint32_t lastAttemptAt = 0;

    static bool retryPending = false;

    // ==================================================
    // Color conversion
    // ==================================================

    static bool parseColor(
        const char *value,
        CRGB &color)
    {

        if (value == nullptr ||
            strlen(value) != 7 ||
            value[0] != '#')
        {

            return false;
        }

        for (size_t i = 1; i < 7; i++)
        {

            if (!isxdigit(
                    static_cast<unsigned char>(value[i])))
            {

                return false;
            }
        }

        uint32_t rgb = strtoul(
            value + 1,
            nullptr,
            16);

        color = CRGB(
            (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF,
            rgb & 0xFF);

        return true;
    }

    static void formatColor(
        CRGB color,
        char *buffer,
        size_t size)
    {

        snprintf(
            buffer,
            size,
            "#%02X%02X%02X",
            color.r,
            color.g,
            color.b);
    }

    // ==================================================
    // Read and validate settings file
    // ==================================================

    static bool readSettingsFile(
        const char *path,
        ThemeManager::Theme &theme)
    {

        File file = LittleFS.open(path, "r");

        if (!file)
        {

            return false;
        }

        if (file.size() > 1024)
        {

            file.close();
            return false;
        }

        StaticJsonDocument<512> doc;

        DeserializationError error =
            deserializeJson(doc, file);

        file.close();

        if (error)
        {

            Serial.print("Settings JSON error: ");
            Serial.println(error.c_str());

            return false;
        }

        if (!doc["version"].is<int>() ||
            doc["version"].as<int>() != 1)
        {

            return false;
        }

        CRGB frontColor;
        CRGB rearColor;

        if (!parseColor(
                doc["front"].as<const char *>(),
                frontColor))
        {

            return false;
        }

        if (!parseColor(
                doc["rear"].as<const char *>(),
                rearColor))
        {

            return false;
        }

        if (!doc["brightness"].is<int>())
        {

            return false;
        }

        int brightness =
            doc["brightness"].as<int>();

        if (brightness < 0 ||
            brightness > 255)
        {

            return false;
        }

        theme.frontColor = frontColor;
        theme.rearColor = rearColor;

        theme.brightness =
            static_cast<uint8_t>(brightness);

        return true;
    }

    // ==================================================
    // Load settings
    // ==================================================

    bool load()
    {

        if (!Storage::isReady())
        {

            return false;
        }

        ThemeManager::Theme theme =
            ThemeManager::getTheme();

        // Try primary settings file

        if (readSettingsFile(
                SETTINGS_PATH,
                theme))
        {

            ThemeManager::setTheme(theme);

            Serial.println("Settings loaded");

            return true;
        }

        // Recover interrupted save

        if (readSettingsFile(
                TEMP_PATH,
                theme))
        {

            ThemeManager::setTheme(theme);

            Serial.println(
                "Settings recovered from temporary file");

            if (LittleFS.exists(SETTINGS_PATH))
            {

                LittleFS.remove(SETTINGS_PATH);
            }

            if (!LittleFS.rename(
                    TEMP_PATH,
                    SETTINGS_PATH))
            {

                Serial.println(
                    "Temporary settings recovery failed");
            }

            return true;
        }

        // Try backup settings file

        if (readSettingsFile(
                BACKUP_PATH,
                theme))
        {

            ThemeManager::setTheme(theme);

            Serial.println(
                "Settings restored from backup");

            return true;
        }

        Serial.println(
            "No valid settings found");

        return false;
    }

    // ==================================================
    // Save settings
    // ==================================================

    bool save()
    {

        if (!Storage::isReady())
        {

            return false;
        }

        if (!LittleFS.exists(CONFIG_DIR))
        {

            if (!LittleFS.mkdir(CONFIG_DIR))
            {

                Serial.println(
                    "Cannot create config directory");

                return false;
            }
        }

        const auto &theme =
            ThemeManager::getTheme();

        char front[8];
        char rear[8];

        formatColor(
            theme.frontColor,
            front,
            sizeof(front));

        formatColor(
            theme.rearColor,
            rear,
            sizeof(rear));

        StaticJsonDocument<512> doc;

        doc["version"] = 1;
        doc["front"] = front;
        doc["rear"] = rear;
        doc["brightness"] = theme.brightness;

        // Write temporary file

        File file = LittleFS.open(
            TEMP_PATH,
            "w");

        if (!file)
        {

            Serial.println(
                "Cannot open temporary settings file");

            return false;
        }

        size_t expected = measureJsonPretty(doc);

        size_t written = serializeJsonPretty(
            doc,
            file);

        file.flush();
        file.close();

        if (written != expected)
        {

            Serial.println(
                "Settings write incomplete");

            LittleFS.remove(TEMP_PATH);

            return false;
        }

        // Verify temporary file

        ThemeManager::Theme verified;

        if (!readSettingsFile(
                TEMP_PATH,
                verified))
        {

            Serial.println(
                "Settings verification failed");

            LittleFS.remove(TEMP_PATH);

            return false;
        }

        // Preserve previous valid settings

        if (LittleFS.exists(SETTINGS_PATH))
        {

            ThemeManager::Theme previous;

            if (readSettingsFile(
                    SETTINGS_PATH,
                    previous))
            {

                if (LittleFS.exists(BACKUP_PATH))
                {

                    if (!LittleFS.remove(BACKUP_PATH))
                    {

                        return false;
                    }
                }

                if (!LittleFS.rename(
                        SETTINGS_PATH,
                        BACKUP_PATH))
                {

                    Serial.println(
                        "Cannot create settings backup");

                    return false;
                }
            }
            else
            {

                // Invalid primary file:
                // preserve existing backup.

                if (!LittleFS.remove(SETTINGS_PATH))
                {

                    return false;
                }
            }
        }

        // Promote temporary file

        if (!LittleFS.rename(
                TEMP_PATH,
                SETTINGS_PATH))
        {

            Serial.println(
                "Cannot finalize settings file");

            return false;
        }

        dirty = false;
        retryPending = false;

        Serial.println("Settings saved");

        return true;
    }

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {

        initialized = false;
        dirty = false;
        retryPending = false;

        if (!Storage::isReady())
        {

            Serial.println(
                "Settings storage unavailable");

            return;
        }

        bool loaded = load();

        initialized = true;

        if (!loaded)
        {

            Serial.println(
                "Using default settings");
        }

        // Save defaults or repair primary file
        // when necessary.

        ThemeManager::Theme verified;

        if (!readSettingsFile(
                SETTINGS_PATH,
                verified))
        {

            markChanged();
        }
    }

    // ==================================================
    // Change notification
    // ==================================================

    void markChanged()
    {

        if (!initialized)
        {

            return;
        }

        dirty = true;
        lastChangeAt = millis();
    }

    // ==================================================
    // Deferred saving
    // ==================================================

    void update()
    {

        if (!initialized ||
            !dirty ||
            !Storage::isReady())
        {

            return;
        }

        uint32_t now = millis();

        if (now - lastChangeAt < SAVE_DELAY)
        {

            return;
        }

        if (retryPending &&
            now - lastAttemptAt < RETRY_DELAY)
        {

            return;
        }

        lastAttemptAt = now;

        if (!save())
        {

            retryPending = true;

            Serial.println(
                "Settings save failed, will retry");
        }
    }

}
