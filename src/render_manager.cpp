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

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {

        lastHour = -1;
        lastMinute = -1;

        lastTimeValid = false;

        lastFrameAt = millis();

        lastStatus = StatusManager::getState();
    }

    // ==================================================
    // Render update
    // ==================================================

    void update()
    {

        uint32_t currentMillis = millis();

        if (currentMillis - lastFrameAt <
            FRAME_INTERVAL)
        {

            return;
        }

        lastFrameAt = currentMillis;

        // Read current time

        TimeManager::ClockTime now;

        bool timeValid =
            TimeManager::getTime(now);

        bool timeChanged = timeValid &&
                           (now.hour != lastHour ||
                            now.minute != lastMinute);

        bool timeValidityChanged =
            timeValid != lastTimeValid;

        // Read current status

        StatusManager::State status =
            StatusManager::getState();

        bool statusChanged =
            status != lastStatus;

        bool statusActive =
            StatusManager::isActive();

        // Check whether rendering is necessary

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

        // Read current theme

        const auto &theme =
            ThemeManager::getTheme();

        // ==============================================
        // Layer 1 - Front display
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
        // Layer 2 - Rear display
        // ==============================================

        Leds::fillRear(theme.rearColor);

        // ==============================================
        // Layer 3 - Status overlay
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

        Leds::setBrightness(theme.brightness);

        // ==============================================
        // Send complete frame
        // ==============================================

        Leds::show();

        // Save rendering state

        lastStatus = status;
        lastTimeValid = timeValid;

        ThemeManager::markRendered();
    }

}