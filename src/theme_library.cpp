
#include "theme_library.h"

#include <FastLED.h>
#include <pgmspace.h>
#include "factory_theme_data.h"

#include "theme_manager.h"
#include "color_manager.h"
#include "transition_manager.h"
#include "custom_theme_store.h"

namespace ThemeLibrary
{

    struct FactoryRecord {
        const char* id;
        const char* json;
        uint32_t front, rear;
        AnimationManager::Effect rearEffect;
        uint8_t rearSpeed;
    };
#define WC_DECLARE_FACTORY(symbol, id, name, front, rear, effect, label, speed) \
    static const char symbol##_json[] PROGMEM = WC_FACTORY_THEME_JSON(id, name, front, rear, label, speed); \
    static_assert(sizeof(symbol##_json) <= FactoryEntryCapacity, "Factory theme JSON too long");
    WC_FACTORY_THEME_ROWS(WC_DECLARE_FACTORY)
#undef WC_DECLARE_FACTORY
#define WC_FACTORY_RECORD(symbol, id, name, front, rear, effect, label, speed) \
    {id, symbol##_json, WC_FACTORY_THEME_RGB(front), WC_FACTORY_THEME_RGB(rear), AnimationManager::Effect::effect, speed},
    static const FactoryRecord FACTORY_THEMES[] PROGMEM = {
        WC_FACTORY_THEME_ROWS(WC_FACTORY_RECORD)
    };
#undef WC_FACTORY_RECORD

    size_t factoryCount() { return sizeof(FACTORY_THEMES)/sizeof(FACTORY_THEMES[0]); }
    static FactoryRecord factoryAt(size_t index) {
        FactoryRecord record;
        memcpy_P(&record, &FACTORY_THEMES[index], sizeof(record));
        return record;
    }
    bool factoryEntry(size_t index, char* output, size_t capacity) {
        if (index >= factoryCount() || !output) return false;
        const auto record = factoryAt(index);
        const size_t length = strlen_P(record.json);
        if (length >= capacity) return false;
        memcpy_P(output, record.json, length+1);
        return true;
    }

    static bool applyResolvedTheme(ThemeManager::Theme theme, const String& id)
    {
        const auto &old = ThemeManager::getTheme();
        theme.brightness = old.brightness;
        const bool different = old.frontBrightness != theme.frontBrightness ||
            old.rearBrightness != theme.rearBrightness || old.frontColor != theme.frontColor ||
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

        for (size_t i=0;i<factoryCount();++i) {
            if (id != factoryAt(i).id) continue;
            ThemeManager::Theme theme = ThemeManager::getTheme();
            if (!resolveFactory(i,theme)) return false;
            return applyResolvedTheme(theme,id);
        }
        return false;
    }

    bool resolveFactory(size_t index, ThemeManager::Theme& theme) {
        if (index >= factoryCount()) return false;
        const auto record = factoryAt(index);
        theme.frontBrightness = 100;
        theme.rearBrightness = 100;
        theme.frontColor = CRGB(record.front);
        theme.rearColor = CRGB(record.rear);
        theme.harmony = ColorManager::Harmony::Manual;
        theme.frontAnimation = AnimationManager::Animation();
        theme.rearAnimation = AnimationManager::Animation();
        theme.rearAnimation.effect = record.rearEffect;
        theme.rearAnimation.speed = record.rearSpeed;
        return true;
    }
}
