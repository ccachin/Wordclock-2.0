
#include "render_manager.h"

#include "theme_manager.h"
#include "automation_manager.h"
#include "weather_manager.h"
#include "time_manager.h"
#include "status_manager.h"
#include "animation_manager.h"
#include "wordclock.h"
#include "leds.h"
#include "config.h"
#include "transition_manager.h"
#include "night_manager.h"
#include "preview_manager.h"
#include "firmware_update.h"
#include "web_update_manager.h"


namespace RenderManager
{

    // ==================================================
    // Rendering configuration
    // ==================================================

    constexpr uint32_t FRAME_INTERVAL = 25;

    // ==================================================
    // Render state
    // ==================================================

    static uint32_t lastFrameAt = 0;
    static DisplayState displayed;
    const DisplayState& displayState() { return displayed; }

    static uint32_t frontStartedAt = 0, rearStartedAt = 0;
    static AnimationManager::Animation previousFront, previousRear;

    static int lastHour = -1;

    static int lastMinute = -1;



    static bool lastTimeValid = false;
    static bool lastWaiting = false;
    static uint32_t waitingStartedAt = 0;
    static bool lastNightActive = false;
    static uint32_t lastNightRevision = 0;
    static uint32_t lastPreviewRevision = 0;
    static uint32_t lastFrontRevision = 0, lastRuleRevision = 0;
    static uint32_t lastWeatherRevision=0;
    static bool lastFrontAllowed = false, lastMaintenance = false;

    static StatusManager::State lastStatus =

        StatusManager::State::Starting;

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {

        TransitionManager::begin();
        NightManager::runDiagnostics();
        lastNightActive = false;
        lastHour = -1;

        lastMinute = -1;



        lastTimeValid = false;
        lastWaiting = false;
        waitingStartedAt = millis();

        lastFrameAt = millis();

        frontStartedAt = rearStartedAt = millis();

        lastStatus =
            StatusManager::getState();

    }

    // ==================================================
    // Rear rendering
    // ==================================================

    static void renderRear(
        const CRGB &baseColor,
        const AnimationManager::Animation &animation,
        uint32_t elapsed)
    {

        switch (animation.effect)
        {

            // Global effects:
            // calculate one color for all rear LEDs.

        case AnimationManager::Effect::Solid:

        case AnimationManager::Effect::Breathing:

        case AnimationManager::Effect::ColorCycle:
        {

            CRGB color =

                AnimationManager::getColor(

                    baseColor,

                    animation,

                    elapsed);

            Leds::fillRear(color);

            break;
        }

            // Spatial effects:
            // calculate the color of each LED.

        case AnimationManager::Effect::Rainbow:

        case AnimationManager::Effect::Chase:
        {

            for (
                uint16_t index = 0;

                index < Config::REAR_LED_COUNT;

                index++)
            {

                CRGB color =

                    AnimationManager::getColor(

                        baseColor,

                        animation,

                        elapsed,

                        index,

                        Config::REAR_LED_COUNT);

                Leds::setRearPixel(

                    index,

                    color);
            }

            break;
        }
        }
    }

    // ==================================================
    // Main render loop
    // ==================================================

    void update()
    {

        if (FirmwareUpdate::active()) return;

        uint32_t currentMillis = millis();

        if (

            currentMillis - lastFrameAt <

            FRAME_INTERVAL

        )
        {

            return;
        }

        lastFrameAt = currentMillis;

        TimeManager::ClockTime now{};

        bool timeValid =

            TimeManager::getTime(now);

        const bool nightActive = NightManager::isActive(timeValid, now.hour, now.minute);
        const uint32_t nightRevision = NightManager::getRevision();
        const bool nightChanged = nightActive != lastNightActive || nightRevision != lastNightRevision;
        if (nightChanged) TransitionManager::cancel();

        bool timeChanged =

            timeValid &&

            (now.hour != lastHour ||

             now.minute != lastMinute);

        bool timeValidityChanged =

            timeValid != lastTimeValid;

        StatusManager::State status =

            StatusManager::getState();

        bool statusChanged =

            status != lastStatus;

        bool statusActive =

            StatusManager::isActive();

        const bool previewActive = PreviewManager::active();
        const uint32_t previewRevision = PreviewManager::revision();
        const bool previewChanged = previewRevision != lastPreviewRevision;
        if (previewActive || previewChanged) TransitionManager::cancel();
        auto theme = PreviewManager::renderTheme();
        const auto& effects = AnimationManager::frontSettings();
        const bool maintenance = WebUpdateManager::active();
        const bool rulesAllowed = !nightActive && timeValid && !previewActive && !maintenance;
        AutomationManager::update(currentMillis,rulesAllowed && theme.brightness && theme.frontBrightness && effects.enabled);
        const bool weatherAllowed=rulesAllowed && !AutomationManager::hasTheme();
        const bool weatherApplied=weatherAllowed && WeatherManager::valid() && WeatherManager::config().intensity;
        const bool weatherFront=weatherApplied && WeatherManager::config().zone!=0;
        const bool programTheme=rulesAllowed && AutomationManager::hasTheme() && theme.brightness;
        WeatherManager::apply(theme,weatherAllowed);
        const uint8_t beforeCap=theme.brightness;
        AutomationManager::apply(theme,rulesAllowed);
        const bool capped=theme.brightness<beforeCap;
        const uint32_t weatherRevision=WeatherManager::revision();
        const uint32_t ruleRevision=AutomationManager::revision();
        const bool allowed = effects.enabled && !nightActive && timeValid && !previewActive &&
            theme.brightness && theme.frontBrightness && !maintenance;
        if (!allowed) AnimationManager::cancelFrontEffect();
        if (allowed && lastTimeValid && timeChanged && !nightChanged && !previewChanged) {
            if (!AnimationManager::frontEffectActive(currentMillis) && FrontEffects::chimeDue(effects.chime,lastHour,lastMinute,now.hour,now.minute,now.second))
                AnimationManager::startFrontEffect(FrontEffects::Cuckoo,currentMillis);
            else if (!AnimationManager::frontEffectActive(currentMillis) &&
                (now.hour!=lastHour || now.minute/5!=lastMinute/5))
                AnimationManager::startFrontEffect(static_cast<FrontEffects::Kind>(effects.transition),currentMillis);
        }
        auto frontAnimation = theme.frontAnimation;
        auto rearAnimation = theme.rearAnimation;
        if (!effects.enabled || maintenance) {
            frontAnimation.effect=rearAnimation.effect=AnimationManager::Effect::Solid;
            TransitionManager::cancel();
        } else if (allowed && !weatherFront && effects.spatial) {
            frontAnimation.effect=effects.spatial==1 ? AnimationManager::Effect::Rainbow : AnimationManager::Effect::Chase;
            frontAnimation.speed=effects.spatialSpeed;
        }
        const bool overlayActive=AnimationManager::frontEffectActive(currentMillis);
        const uint32_t frontRevision=AnimationManager::frontRevision();

        if (frontAnimation.effect != previousFront.effect || frontAnimation.speed != previousFront.speed)
            frontStartedAt = currentMillis;
        if (rearAnimation.effect != previousRear.effect || rearAnimation.speed != previousRear.speed)
            rearStartedAt = currentMillis;
        previousFront = frontAnimation;
        previousRear = rearAnimation;
        bool animationActive = AnimationManager::isAnimated(frontAnimation) ||
                               AnimationManager::isAnimated(rearAnimation);

        const bool waiting = !timeValid && status != StatusManager::State::WifiSetupRequired;
        if (waiting && !lastWaiting) waitingStartedAt = currentMillis;
        // Do not blend a theme transition into the waiting indicator.
        if (!timeValid) TransitionManager::cancel();
        const bool fadeInTime = effects.enabled && !maintenance && timeValid && lastWaiting && !previewActive && !nightActive;
        if (fadeInTime) TransitionManager::request();

        // Render only when necessary.

        bool needsRender =

            weatherRevision != lastWeatherRevision || ruleRevision != lastRuleRevision || overlayActive || frontRevision != lastFrontRevision || allowed != lastFrontAllowed || maintenance != lastMaintenance ||
            waiting ||
            waiting != lastWaiting ||
            previewChanged ||
            nightChanged ||
            timeChanged ||

            timeValidityChanged ||

            statusChanged ||

            statusActive ||

            animationActive ||
            TransitionManager::isActive() ||

            ThemeManager::hasChanged();

        if (!needsRender)
        {

            return;
        }



        // ==============================================
        // Front
        // ==============================================
        const CRGB frontBase=allowed && !weatherFront ? AnimationManager::hourlyPalette(theme.frontColor,now.hour) : theme.frontColor;

        if (timeValid)
        {

            Wordclock::displayTime(

                now.hour,

                now.minute,

                AnimationManager::getColor(frontBase, frontAnimation, currentMillis - frontStartedAt));
            if (frontAnimation.effect == AnimationManager::Effect::Rainbow || frontAnimation.effect == AnimationManager::Effect::Chase) {
                for (uint16_t i=0;i<Config::FRONT_LED_COUNT-4;++i) {
                    if (Leds::getPixel(i)!=CRGB::Black)
                        Leds::setPixel(i,AnimationManager::getColor(frontBase,frontAnimation,currentMillis-frontStartedAt,i,Config::FRONT_LED_COUNT-4));
                }
            }

            lastHour = now.hour;

            lastMinute = now.minute;
        }
        else
        {

            Leds::clearFront();
            if (waiting) {
                // Crossfade between adjacent minute dots, one cycle per 1600 ms.
                const uint32_t phase = (currentMillis - waitingStartedAt) % 1600UL;
                const uint8_t dot = phase / 400UL;
                const uint8_t fraction = (phase % 400UL) * 255UL / 400UL;
                const uint8_t nextLevel = ease8InOutQuad(fraction);
                CRGB a = CRGB(255, 225, 180);
                CRGB b = CRGB(255, 225, 180);
                a.nscale8_video((uint8_t)((255U - nextLevel) * 102U / 255U));
                b.nscale8_video((uint8_t)(nextLevel * 102U / 255U));
                Leds::setPixel(Config::MINUTE_LEDS[dot], a);
                Leds::setPixel(Config::MINUTE_LEDS[(dot + 1) % 4], b);
            }
        }

        // ==============================================
        // Rear
        // ==============================================

        renderRear(

            ColorManager::resolveRearColor(frontBase, theme.rearColor, theme.harmony),

            rearAnimation,

            currentMillis - rearStartedAt);

        // ==============================================
        // Status overlay
        // ==============================================

        uint8_t brightness = theme.brightness;
        uint8_t frontBrightness = theme.frontBrightness;
        uint8_t rearBrightness = theme.rearBrightness;
        if (!previewActive && !previewChanged) TransitionManager::apply(currentMillis, timeChanged, brightness, frontBrightness, rearBrightness);
        TransitionManager::remember(brightness, frontBrightness, rearBrightness);
        if (overlayActive && allowed) AnimationManager::applyFrontEffect(currentMillis,frontBase);

        if (nightActive) {
            const auto& night = NightManager::getSettings();
            if (brightness > night.brightness) brightness = night.brightness;
            if (night.mode == NightManager::Mode::FrontOnly) Leds::clearRear();
            if (night.mode == NightManager::Mode::Off) { Leds::clear(); brightness = 0; }
        }

        if (statusActive && !nightActive)
        {

            Leds::setRearPixel(

                StatusManager::STATUS_LED_INDEX,

                StatusManager::getColor());
        }

        // ==============================================
        // Global brightness
        // ==============================================

        // Scale after transition snapshots and night/status processing.
        // Keep snapshots unscaled to avoid cumulative attenuation.
        for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i) {
            const uint8_t percent = i < Config::FRONT_LED_COUNT ? frontBrightness : rearBrightness;
            if (percent == 100) continue;
            CRGB c = Leds::getPixel(i);
            c.r = (uint16_t(c.r) * percent + 50) / 100;
            c.g = (uint16_t(c.g) * percent + 50) / 100;
            c.b = (uint16_t(c.b) * percent + 50) / 100;
            Leds::setPixel(i, c);
        }
        // Use full controller brightness only for the system indicator frame.
        // Preserve the normal rear output by applying its global scale in software.
        // A zero global brightness (off) or zero front brightness still hides the dots.
        if (waiting && brightness > 0 && frontBrightness > 0 && !nightActive) {
            for (uint16_t i = Config::REAR_LED_START; i < Config::TOTAL_LED_COUNT; ++i) {
                CRGB c = Leds::getPixel(i);
                c.nscale8(brightness);
                Leds::setPixel(i, c);
            }
            // Rebuild the indicator after ordinary theme scaling.
            const uint32_t phase = (currentMillis - waitingStartedAt) % 1600UL;
            const uint8_t dot = phase / 400UL;
            const uint8_t level = ease8InOutQuad((phase % 400UL) * 255UL / 400UL);
            CRGB a(255, 225, 180), b(255, 225, 180);
            a.nscale8_video((255U - level) * 102U / 255U);
            b.nscale8_video(level * 102U / 255U);
            Leds::clearFront();
            Leds::setPixel(Config::MINUTE_LEDS[dot], a);
            Leds::setPixel(Config::MINUTE_LEDS[(dot + 1) % 4], b);
            Leds::setBrightness(255);
        } else {
            Leds::setBrightness(brightness);
        }

        Leds::show();
        const auto& weatherConfig=WeatherManager::config();
        displayed=describeFrame(theme.brightness!=0,nightActive,timeValid,previewActive,
            programTheme,weatherApplied,capped,weatherConfig.enabled,WeatherManager::valid(),
            weatherConfig.intensity!=0,weatherConfig.zone,maintenance,
            animationActive || overlayActive || waiting || TransitionManager::isActive() || statusActive);

        lastStatus = status;

        lastTimeValid = timeValid;
        lastWaiting = waiting;
        lastNightActive = nightActive;
        lastNightRevision = nightRevision;
        lastPreviewRevision = previewRevision;
        lastFrontRevision = frontRevision;
        lastRuleRevision = ruleRevision;
        lastWeatherRevision = weatherRevision;
        lastFrontAllowed = allowed;
        lastMaintenance = maintenance;

        ThemeManager::markRendered();
    }

}
