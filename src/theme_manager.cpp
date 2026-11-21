#include "theme_manager.h"
#include "config.h"

namespace ThemeManager
{

    static Theme currentTheme;

    static bool changed = true;

    void begin()
    {

        currentTheme.frontColor = CRGB(255, 225, 180);
        currentTheme.rearColor = CRGB(60, 100, 180);
        currentTheme.brightness =
            Config::DEFAULT_BRIGHTNESS;

        changed = true;
    }

    const Theme &getTheme()
    {

        return currentTheme;
    }

    void setFrontColor(CRGB color)
    {

        currentTheme.frontColor = color;
        changed = true;
    }

    void setRearColor(CRGB color)
    {

        currentTheme.rearColor = color;
        changed = true;
    }

    void setBrightness(uint8_t brightness)
    {

        currentTheme.brightness = brightness;
        changed = true;
    }

    void setTheme(const Theme &theme)
    {

        currentTheme = theme;
        changed = true;
    }

    bool hasChanged()
    {

        return changed;
    }

    void markRendered()
    {

        changed = false;
    }

}