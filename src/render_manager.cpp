#include "render_manager.h"
#include "theme_manager.h"
#include "time_manager.h"
#include "wordclock.h"
#include "leds.h"

namespace RenderManager
{

    static int lastHour = -1;
    static int lastMinute = -1;

    void begin()
    {

        lastHour = -1;
        lastMinute = -1;
    }

    void update()
    {

        TimeManager::ClockTime now;

        bool timeValid = TimeManager::getTime(now);

        bool timeChanged = timeValid &&
                           (now.hour != lastHour ||
                            now.minute != lastMinute);

        if (!timeChanged &&
            !ThemeManager::hasChanged())
        {

            return;
        }

        const auto &theme =
            ThemeManager::getTheme();

        Leds::setBrightness(theme.brightness);

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

        Leds::fillRear(theme.rearColor);

        ThemeManager::markRendered();

        Leds::show();
    }

}