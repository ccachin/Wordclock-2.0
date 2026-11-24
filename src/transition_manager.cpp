#include "transition_manager.h"
#include "config.h"
#include "leds.h"
#include "color_manager.h"
#include "settings_manager.h"
namespace TransitionManager {
    static uint16_t duration = DEFAULT_DURATION;
    static uint16_t activeDuration = DEFAULT_DURATION;
    uint16_t getDuration() { return duration; }
    void setDuration(uint16_t value) {
        if (value > MAX_DURATION || value == duration) return;
        duration = value;
        SettingsManager::markChanged();
    }
    static CRGB last[Config::TOTAL_LED_COUNT];
    static CRGB source[Config::TOTAL_LED_COUNT];
    static CRGB lastFront, sourceFront;
    static uint8_t lastBrightness, sourceBrightness;
    static uint8_t lastFrontBrightness, lastRearBrightness, sourceFrontBrightness, sourceRearBrightness;
    static bool ready = false, active = false;
    static uint32_t started;
    void begin() { ready = active = false; }
    void cancel() { active = false; }
    bool isActive() { return active; }
    void request() {
        if (!ready || duration == 0) { active = false; return; }
        activeDuration = duration;
        for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i) source[i] = last[i];
        sourceFront = lastFront;
        sourceBrightness = lastBrightness;
        sourceFrontBrightness = lastFrontBrightness;
        sourceRearBrightness = lastRearBrightness;
        started = millis();
        active = true;
    }
    void apply(uint32_t now, bool timeChanged, uint8_t& brightness, uint8_t& frontBrightness, uint8_t& rearBrightness) {
        if (!active) return;
        uint32_t elapsed = now - started;
        if (elapsed >= activeDuration) { active = false; return; }
        float t = static_cast<float>(elapsed) / activeDuration;
        t = t*t*(3.0f-2.0f*t);
        for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i) {
            CRGB target = Leds::getPixel(i);
            CRGB from = source[i];
            if (i < Config::FRONT_LED_COUNT) {
                // Never retain letters that are not part of the current time.
                if (target == CRGB::Black) continue;
                if (timeChanged || from == CRGB::Black) from = sourceFront;
            }
            Leds::setPixel(i, ColorManager::interpolate(from, target, t));
        }
        brightness = static_cast<uint8_t>(sourceBrightness + (static_cast<int>(brightness)-sourceBrightness)*t + 0.5f);
        frontBrightness = static_cast<uint8_t>(sourceFrontBrightness + (static_cast<int>(frontBrightness)-sourceFrontBrightness)*t + 0.5f);
        rearBrightness = static_cast<uint8_t>(sourceRearBrightness + (static_cast<int>(rearBrightness)-sourceRearBrightness)*t + 0.5f);
    }
    void remember(uint8_t brightness, uint8_t frontBrightness, uint8_t rearBrightness) {
        lastFront = CRGB::Black;
        for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i) {
            last[i] = Leds::getPixel(i);
            if (i < Config::FRONT_LED_COUNT && last[i] != CRGB::Black) lastFront = last[i];
        }
        lastBrightness = brightness;
        lastFrontBrightness = frontBrightness;
        lastRearBrightness = rearBrightness;
        ready = true;
    }
}
