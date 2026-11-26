
#include "animation_manager.h"

#include "color_manager.h"
#include "settings_manager.h"
#include "leds.h"
#include "config.h"

namespace AnimationManager
{

    static FrontEffects::Settings frontConfig;
    static uint32_t revision = 0, effectStart = 0;
    static FrontEffects::Kind running = FrontEffects::None;
    const FrontEffects::Settings& frontSettings() { return frontConfig; }
    uint32_t frontRevision() { return revision; }
    bool setFrontSettings(const FrontEffects::Settings& value) {
        if (!FrontEffects::valid(value)) return false;
        if (FrontEffects::pack(value) != FrontEffects::pack(frontConfig)) {
            frontConfig=value; running=FrontEffects::None; ++revision;
            SettingsManager::markChanged();
        }
        return true;
    }
    bool startFrontEffect(FrontEffects::Kind kind, uint32_t now) {
        if (!frontConfig.enabled || !frontConfig.intensity || kind <= FrontEffects::None || kind > FrontEffects::Cuckoo) return false;
        running=kind; effectStart=now; ++revision; return true;
    }
    void cancelFrontEffect() { if (running != FrontEffects::None) { running=FrontEffects::None; ++revision; } }
    bool frontEffectActive(uint32_t now) {
        if (running != FrontEffects::None && now-effectStart >= frontConfig.duration) cancelFrontEffect();
        return running != FrontEffects::None;
    }
    CRGB hourlyPalette(const CRGB& base, uint8_t hour) {
        if (!frontConfig.enabled || !frontConfig.hourlyColor) return base;
        const uint8_t peak=base.r>base.g ? (base.r>base.b?base.r:base.b) : (base.g>base.b?base.g:base.b);
        CRGB target; hsv2rgb_rainbow(CHSV(uint8_t(hour*256U/24U),255,peak),target);
        return ColorManager::interpolate(base,target,frontConfig.intensity/100.0f);
    }
    void applyFrontEffect(uint32_t now, const CRGB& color) {
        if (!frontEffectActive(now)) return;
        const uint8_t p=FrontEffects::progress(now-effectStart,frontConfig.duration);
        for (uint16_t i=0;i<Config::FRONT_LED_COUNT-4;++i) {
            CRGB c=Leds::getPixel(i);
            if (c != CRGB::Black) {
                // Keep time legible; even a full-strength transition retains a 25% floor.
                const unsigned reveal=FrontEffects::reveal(running,i,p);
                const uint8_t level=255-(255-reveal)*frontConfig.intensity*75UL/10000UL;
                c.nscale8_video(level);
            } else {
                const uint8_t level=FrontEffects::overlay(running,i,p)*frontConfig.intensity/100U;
                if (!level) continue;
                c=color; c.nscale8_video(level);
            }
            Leds::setPixel(i,c);
        }
    }

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
