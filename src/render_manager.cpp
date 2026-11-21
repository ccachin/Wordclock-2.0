
#include "render_manager.h"

#include "theme_manager.h"
#include "time_manager.h"
#include "status_manager.h"
#include "wordclock.h"
#include "leds.h"

namespace RenderManager
{

    constexpr uint32_t FRAME_INTERVAL = 25;

    static uint32_t lastFrameAt = 0;

    static int lastHour = -1;
    static int lastMinute = -1;

    static bool lastTimeValid = false;

    static StatusManager::State lastStatus =
        StatusManager::State::Starting;

    void begin()
    {

        lastHour = -1;

        lastMinute = -1;

        lastTimeValid = false;

        lastFrameAt = millis();

        lastStatus =
            StatusManager::getState();
    }

    void update()
    {

        uint32_t currentMillis = millis();

        if (
            currentMillis - lastFrameAt <
            FRAME_INTERVAL)
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

        bool needsRender =

            timeChanged ||

            timeValidityChanged ||

            statusChanged ||

            statusActive ||

            ThemeManager::hasChanged();

        if (!needsRender)
        {

            return;
        }

        const auto &theme =
            ThemeManager::getTheme();

        // Front

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

        // Rear

        Leds::fillRear(
            ThemeManager::getEffectiveRearColor());

        // Status overlay

        if (statusActive)
        {

            Leds::setRearPixel(
                StatusManager::STATUS_LED_INDEX,
                StatusManager::getColor());
        }

        // Global brightness

        Leds::setBrightness(
            theme.brightness);

        Leds::show();

        lastStatus = status;

        lastTimeValid = timeValid;

        ThemeManager::markRendered();
    }

}
