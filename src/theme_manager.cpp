
#include "theme_manager.h"

#include "config.h"
#include "settings_manager.h"

namespace ThemeManager
{

    static Theme currentTheme;

    static bool changed = true;

    // ==================================================
    // Change notification
    // ==================================================

    static void notifyChange()
    {

        changed = true;

        SettingsManager::markChanged();
    }

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {

        currentTheme.frontColor =
            CRGB(255, 225, 180);

        currentTheme.rearColor =
            CRGB(60, 100, 180);

        currentTheme.brightness =
            Config::DEFAULT_BRIGHTNESS;

        changed = true;
    }

    // ==================================================
    // Read current theme
    // ==================================================

    const Theme &getTheme()
    {

        return currentTheme;
    }

    // ==================================================
    // Front color
    // ==================================================

    void setFrontColor(CRGB color)
    {

        if (currentTheme.frontColor == color)
        {

            return;
        }

        currentTheme.frontColor = color;

        notifyChange();
    }

    // ==================================================
    // Rear color
    // ==================================================

    void setRearColor(CRGB color)
    {

        if (currentTheme.rearColor == color)
        {

            return;
        }

        currentTheme.rearColor = color;

        notifyChange();
    }

    // ==================================================
    // Brightness
    // ==================================================

    void setBrightness(uint8_t brightness)
    {

        if (brightness > Config::MAX_BRIGHTNESS)
        {

            brightness = Config::MAX_BRIGHTNESS;
        }

        if (currentTheme.brightness == brightness)
        {

            return;
        }

        currentTheme.brightness = brightness;

        notifyChange();
    }

    // ==================================================
    // Complete theme
    // ==================================================

    void setTheme(const Theme &theme)
    {

        Theme nextTheme = theme;

        if (nextTheme.brightness >
            Config::MAX_BRIGHTNESS)
        {

            nextTheme.brightness =
                Config::MAX_BRIGHTNESS;
        }

        if (
            currentTheme.frontColor ==
                nextTheme.frontColor &&

            currentTheme.rearColor ==
                nextTheme.rearColor &&

            currentTheme.brightness ==
                nextTheme.brightness)
        {

            return;
        }

        currentTheme = nextTheme;

        notifyChange();
    }

    // ==================================================
    // Rendering state
    // ==================================================

    bool hasChanged()
    {

        return changed;
    }

    void markRendered()
    {

        changed = false;
    }

}
