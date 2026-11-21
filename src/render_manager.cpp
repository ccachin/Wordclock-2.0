
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

    // Temporary animation demonstration.

    constexpr bool ANIMATION_DEMO = true;

    constexpr uint32_t DEMO_DURATION = 20000;

    constexpr uint8_t DEMO_SPEED = 75;

    static const AnimationManager::Effect DEMO_EFFECTS[] = {

        AnimationManager::Effect::Solid,

        AnimationManager::Effect::Breathing,

        AnimationManager::Effect::ColorCycle,

        AnimationManager::Effect::Rainbow,

        AnimationManager::Effect::Chase

    };

    constexpr size_t DEMO_EFFECT_COUNT =

        sizeof(DEMO_EFFECTS) /
        sizeof(DEMO_EFFECTS[0]);

    // ==================================================
    // Render state
    // ==================================================

    static uint32_t lastFrameAt = 0;

    static uint32_t demoStartedAt = 0;

    static int lastHour = -1;

    static int lastMinute = -1;

    static int lastDemoEffect = -1;

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

        lastDemoEffect = -1;

        lastTimeValid = false;

        lastFrameAt = millis();

        demoStartedAt = millis();

        lastStatus =
            StatusManager::getState();

        if (ANIMATION_DEMO)
        {

            Serial.println();

            Serial.println(
                "Animation demo enabled");

            Serial.println(
                "Each effect runs for 20 seconds");
        }
    }

    // ==================================================
    // Temporary demo animation
    // ==================================================

    static AnimationManager::Animation getRearAnimation(
        uint32_t now,
        uint32_t &elapsed,
        bool &effectChanged)
    {

        AnimationManager::Animation animation;

        elapsed = 0;

        effectChanged = false;

        if (!ANIMATION_DEMO)
        {

            return animation;
        }

        uint32_t demoElapsed =

            now - demoStartedAt;

        int index =

            (demoElapsed / DEMO_DURATION)

            % DEMO_EFFECT_COUNT;

        elapsed =

            demoElapsed % DEMO_DURATION;

        animation.effect =

            DEMO_EFFECTS[index];

        animation.speed = DEMO_SPEED;

        if (index != lastDemoEffect)
        {

            lastDemoEffect = index;

            effectChanged = true;

            Serial.print(
                "Animation demo: ");

            Serial.println(

                AnimationManager::effectToString(
                    animation.effect));
        }

        return animation;
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

        // Get temporary animation parameters.

        uint32_t animationElapsed = 0;

        bool effectChanged = false;

        AnimationManager::Animation rearAnimation =

            getRearAnimation(

                currentMillis,

                animationElapsed,

                effectChanged);

        bool animationActive =

            AnimationManager::isAnimated(
                rearAnimation);

        // Render only when necessary.

        bool needsRender =

            timeChanged ||

            timeValidityChanged ||

            statusChanged ||

            statusActive ||

            effectChanged ||

            animationActive ||

            ThemeManager::hasChanged();

        if (!needsRender)
        {

            return;
        }

        const auto &theme =

            ThemeManager::getTheme();

        // ==============================================
        // Front
        // ==============================================

        if (timeValid)
        {

            Wordclock::displayTime(

                now.hour,

                now.minute,

                theme.frontColor);

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

            animationElapsed);

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
