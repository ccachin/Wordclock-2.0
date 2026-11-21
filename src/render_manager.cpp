
#include "render_manager.h"

#include "theme_manager.h"
#include "time_manager.h"
#include "status_manager.h"
#include "animation_manager.h"
#include "wordclock.h"
#include "leds.h"
#include "config.h"

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

    static StatusManager::State lastStatus =

        StatusManager::State::Starting;

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {

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

        TimeManager::ClockTime now;

        bool timeValid =

            TimeManager::getTime(now);

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

            timeChanged ||

            timeValidityChanged ||

            statusChanged ||

            statusActive ||

            animationActive ||

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

        if (statusActive)
        {

            Leds::setRearPixel(

                StatusManager::STATUS_LED_INDEX,

                StatusManager::getColor());
        }

        // ==============================================
        // Global brightness
        // ==============================================

        Leds::setBrightness(

            theme.brightness);

        Leds::show();

        lastStatus = status;

        lastTimeValid = timeValid;

        ThemeManager::markRendered();
    }

}
