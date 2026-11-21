
#include "render_manager.h"

#include "theme_manager.h"
#include "time_manager.h"
#include "status_manager.h"
#include "animation_manager.h"
#include "wordclock.h"
#include "leds.h"
#include "config.h"
#include "transition_manager.h"
#include "night_manager.h"


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

    static uint32_t frontStartedAt = 0, rearStartedAt = 0;
    static AnimationManager::Animation previousFront, previousRear;

    static int lastHour = -1;

    static int lastMinute = -1;



    static bool lastTimeValid = false;
    static bool lastNightActive = false;
    static uint32_t lastNightRevision = 0;

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

        const auto &theme = ThemeManager::getTheme();
        const auto &frontAnimation = theme.frontAnimation;
        const auto &rearAnimation = theme.rearAnimation;
        if (frontAnimation.effect != previousFront.effect || frontAnimation.speed != previousFront.speed)
            frontStartedAt = currentMillis;
        if (rearAnimation.effect != previousRear.effect || rearAnimation.speed != previousRear.speed)
            rearStartedAt = currentMillis;
        previousFront = frontAnimation;
        previousRear = rearAnimation;
        bool animationActive = AnimationManager::isAnimated(frontAnimation) ||
                               AnimationManager::isAnimated(rearAnimation);

        // Render only when necessary.

        bool needsRender =

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

        if (timeValid)
        {

            Wordclock::displayTime(

                now.hour,

                now.minute,

                AnimationManager::getColor(theme.frontColor, frontAnimation, currentMillis - frontStartedAt));

            lastHour = now.hour;

            lastMinute = now.minute;
        }
        else
        {

            Leds::clearFront();
        }

        // ==============================================
        // Rear
        // ==============================================

        renderRear(

            ThemeManager::getEffectiveRearColor(),

            rearAnimation,

            currentMillis - rearStartedAt);

        // ==============================================
        // Status overlay
        // ==============================================

        uint8_t brightness = theme.brightness;
        TransitionManager::apply(currentMillis, timeChanged, brightness);
        TransitionManager::remember(brightness);

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

        Leds::setBrightness(

            brightness);

        Leds::show();

        lastStatus = status;

        lastTimeValid = timeValid;
        lastNightActive = nightActive;
        lastNightRevision = nightRevision;

        ThemeManager::markRendered();
    }

}
