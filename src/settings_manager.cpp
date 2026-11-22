
#include "settings_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ctype.h>
#include <stdlib.h>

#include "storage.h"
#include "theme_manager.h"
#include "color_manager.h"
#include "transition_manager.h"
#include "night_manager.h"

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

    constexpr int SETTINGS_VERSION = 7;

    constexpr uint32_t SAVE_DELAY = 3000;
    constexpr uint32_t RETRY_DELAY = 10000;

    static uint32_t faceRgb = 0x151515;
    uint32_t faceColor() { return faceRgb; }
    void setFaceColor(uint32_t rgb) { if (rgb <= 0xFFFFFF && rgb != faceRgb) { faceRgb = rgb; markChanged(); } }

    static bool dirty = false;
    static bool initialized = false;
    static bool retryPending = false;
    static bool migrationNeeded = false;

    static uint32_t lastChangeAt = 0;
    static uint32_t lastAttemptAt = 0;

    // ==================================================
    // Color conversion
    // ==================================================

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
                    static_cast<unsigned char>(value[i])))
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
    // Read settings
    // ==================================================

    static bool readSettingsFile(
        const char *path,
        ThemeManager::Theme &theme,
        int *loadedVersion = nullptr, uint16_t *transitionDuration = nullptr, NightManager::Settings *nightOut = nullptr, uint32_t *faceOut = nullptr)
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

        StaticJsonDocument<1024> doc;

        DeserializationError error =
            deserializeJson(doc, file);

        file.close();

        if (error)
        {

            Serial.print("Settings JSON error: ");
            Serial.println(error.c_str());

            return false;
        }

        if (!doc["version"].is<int>())
        {
            return false;
        }

        int version = doc["version"].as<int>();

        if (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != 6 && version != 7)
        {

            Serial.println(
                "Unsupported settings version");

            return false;
        }

        CRGB front;
        CRGB rear;

        if (!parseColor(
                doc["front"].as<const char *>(),
                front))
        {
            return false;
        }

        if (!parseColor(
                doc["rear"].as<const char *>(),
                rear))
        {
            return false;
        }

        if (!doc["brightness"].is<int>())
        {
            return false;
        }

        int brightness =
            doc["brightness"].as<int>();

        if (brightness < 0 || brightness > 255)
        {
            return false;
        }

        ColorManager::Harmony harmony =
            ColorManager::Harmony::Manual;

        if (version >= 2)
        {

            const char *name =
                doc["harmony"].as<const char *>();

            if (
                name == nullptr ||
                !ColorManager::harmonyFromString(
                    String(name),
                    harmony))
            {
                return false;
            }
        }

        theme.frontColor = front;
        theme.rearColor = rear;

        theme.brightness =
            static_cast<uint8_t>(brightness);

        theme.frontBrightness = theme.rearBrightness = 100;
        if (version >= 6) {
            if (!doc["frontBrightness"].is<int>() || !doc["rearBrightness"].is<int>()) return false;
            int f = doc["frontBrightness"].as<int>(), r = doc["rearBrightness"].as<int>();
            if (f < 0 || f > 100 || r < 0 || r > 100) return false;
            theme.frontBrightness = f; theme.rearBrightness = r;
        }
        theme.harmony = harmony;
        theme.frontAnimation = AnimationManager::Animation();
        theme.rearAnimation = AnimationManager::Animation();
        if (version >= 3) {
            const char* f = doc["frontEffect"].as<const char*>();
            const char* r = doc["rearEffect"].as<const char*>();
            if (!f || !r || !AnimationManager::effectFromString(String(f), theme.frontAnimation.effect) ||
                !AnimationManager::effectFromString(String(r), theme.rearAnimation.effect)) return false;
            if (theme.frontAnimation.effect == AnimationManager::Effect::Rainbow ||
                theme.frontAnimation.effect == AnimationManager::Effect::Chase) return false;
            if (!doc["frontSpeed"].is<int>() || !doc["rearSpeed"].is<int>()) return false;
            int fs = doc["frontSpeed"].as<int>(), rs = doc["rearSpeed"].as<int>();
            if (fs < 0 || fs > 100 || rs < 0 || rs > 100) return false;
            theme.frontAnimation.speed = fs;
            theme.rearAnimation.speed = rs;
        }

        uint16_t duration = TransitionManager::DEFAULT_DURATION;
        if (version >= 4) {
            if (!doc["transitionDuration"].is<int>()) return false;
            int value = doc["transitionDuration"].as<int>();
            if (value < 0 || value > TransitionManager::MAX_DURATION) return false;
            duration = value;
        }
        NightManager::Settings night;
        if (version >= 5) {
            if (!doc["nightEnabled"].is<bool>() || !doc["nightStart"].is<int>() ||
                !doc["nightEnd"].is<int>() || !doc["nightBrightness"].is<int>() || !doc["nightMode"].is<int>()) return false;
            int start = doc["nightStart"], end = doc["nightEnd"], bright = doc["nightBrightness"], mode = doc["nightMode"];
            if (start < 0 || start >= 1440 || end < 0 || end >= 1440 || bright < 1 || bright > 255 || mode < 0 || mode > 2) return false;
            night.enabled = doc["nightEnabled"].as<bool>();
            night.startMinute = start; night.endMinute = end; night.brightness = bright;
            night.mode = static_cast<NightManager::Mode>(mode);
        }
        CRGB face(0x15, 0x15, 0x15);
        if (version >= 7 && !parseColor(doc["faceColor"].as<const char *>(), face)) return false;
        if (faceOut) *faceOut = (uint32_t(face.r)<<16) | (uint32_t(face.g)<<8) | face.b;
        if (nightOut) *nightOut = night;
        if (transitionDuration) *transitionDuration = duration;

        if (loadedVersion != nullptr)
        {
            *loadedVersion = version;
        }

        return true;
    }

    // ==================================================
    // Load and recovery
    // ==================================================

    bool load()
    {

        if (!Storage::isReady())
        {
            return false;
        }

        ThemeManager::Theme theme =
            ThemeManager::getTheme();

        int version = 0;
        NightManager::Settings night;
        uint16_t duration = TransitionManager::DEFAULT_DURATION;

        if (readSettingsFile(
                SETTINGS_PATH,
                theme,
                &version, &duration, &night, &faceRgb))
        {

            ThemeManager::setTheme(theme);
            TransitionManager::setDuration(duration);
            NightManager::setSettings(night);

            migrationNeeded =
                version < SETTINGS_VERSION;

            Serial.printf(
                "Settings loaded (version %d)\n",
                version);

            return true;
        }

        if (readSettingsFile(
                TEMP_PATH,
                theme,
                &version, &duration, &night, &faceRgb))
        {

            ThemeManager::setTheme(theme);
            TransitionManager::setDuration(duration);
            NightManager::setSettings(night);

            migrationNeeded = true;

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

        if (readSettingsFile(
                BACKUP_PATH,
                theme,
                &version, &duration, &night, &faceRgb))
        {

            ThemeManager::setTheme(theme);
            TransitionManager::setDuration(duration);
            NightManager::setSettings(night);

            migrationNeeded = true;

            Serial.println(
                "Settings restored from backup");

            return true;
        }

        Serial.println("No valid settings found");

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

        StaticJsonDocument<1024> doc;

        char face[8];
        snprintf(face, sizeof(face), "#%06lX", static_cast<unsigned long>(faceRgb));
        doc["faceColor"] = face;
        doc["version"] = SETTINGS_VERSION;
        const auto night = NightManager::getSettings();
        doc["nightEnabled"] = night.enabled;
        doc["nightStart"] = night.startMinute;
        doc["nightEnd"] = night.endMinute;
        doc["nightBrightness"] = night.brightness;
        doc["nightMode"] = static_cast<uint8_t>(night.mode);
        doc["transitionDuration"] = TransitionManager::getDuration();
        doc["front"] = front;
        doc["rear"] = rear;
        doc["brightness"] = theme.brightness;
        doc["frontBrightness"] = theme.frontBrightness;
        doc["rearBrightness"] = theme.rearBrightness;

        doc["harmony"] =
            ColorManager::harmonyToString(
                theme.harmony);

        doc["frontEffect"] = AnimationManager::effectToString(theme.frontAnimation.effect);
        doc["rearEffect"] = AnimationManager::effectToString(theme.rearAnimation.effect);
        doc["frontSpeed"] = theme.frontAnimation.speed;
        doc["rearSpeed"] = theme.rearAnimation.speed;
        File file =
            LittleFS.open(TEMP_PATH, "w");

        if (!file)
        {

            Serial.println(
                "Cannot open temporary settings file");

            return false;
        }

        size_t expected =
            measureJsonPretty(doc);

        size_t written =
            serializeJsonPretty(doc, file);

        file.flush();
        file.close();

        if (written != expected)
        {

            Serial.println(
                "Settings write incomplete");

            LittleFS.remove(TEMP_PATH);

            return false;
        }

        ThemeManager::Theme verified;
        uint16_t verifiedDuration = 0;
        NightManager::Settings verifiedNight;
        uint32_t verifiedFace = 0;

        if (!readSettingsFile(
                TEMP_PATH,
                verified, nullptr, &verifiedDuration, &verifiedNight, &verifiedFace))
        {

            Serial.println(
                "Settings verification failed");

            LittleFS.remove(TEMP_PATH);

            return false;
        }

        if (
            verifiedFace != faceRgb ||
            !NightManager::equals(night, verifiedNight) ||
            verifiedDuration != TransitionManager::getDuration() ||
            verified.frontColor != theme.frontColor ||
            verified.rearColor != theme.rearColor ||
            verified.brightness != theme.brightness ||
            verified.frontBrightness != theme.frontBrightness ||
            verified.rearBrightness != theme.rearBrightness ||
            verified.harmony != theme.harmony ||
            verified.frontAnimation.effect != theme.frontAnimation.effect ||
            verified.rearAnimation.effect != theme.rearAnimation.effect ||
            verified.frontAnimation.speed != theme.frontAnimation.speed ||
            verified.rearAnimation.speed != theme.rearAnimation.speed)
        {

            Serial.println(
                "Settings content mismatch");

            LittleFS.remove(TEMP_PATH);

            return false;
        }

        // Preserve previous valid settings.

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

                if (!LittleFS.remove(SETTINGS_PATH))
                {
                    return false;
                }
            }
        }

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
        migrationNeeded = false;

        Serial.printf(
            "Settings saved (version %d)\n",
            SETTINGS_VERSION);

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
        migrationNeeded = false;

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
            Serial.println("Using default settings");
        }

        if (migrationNeeded)
        {

            Serial.println(
                "Settings migration scheduled");
        }

        if (!loaded || migrationNeeded)
        {
            markChanged();
        }
    }

    // ==================================================
    // Deferred saving
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

    void update()
    {

        if (
            !initialized ||
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

        if (
            retryPending &&
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
