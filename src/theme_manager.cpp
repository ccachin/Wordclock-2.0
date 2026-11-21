
#include "theme_manager.h"

#include "config.h"
#include "settings_manager.h"
#include "transition_manager.h"

namespace ThemeManager
{

    static Theme currentTheme;

    static CRGB effectiveRearColor;

    static bool changed = true;

    // ==================================================
    // Internal helpers
    // ==================================================

    static void updateEffectiveColors()
    {

        effectiveRearColor =
            ColorManager::resolveRearColor(
                currentTheme.frontColor,
                currentTheme.rearColor,
                currentTheme.harmony);
    }

    static void notifyChange()
    {

        TransitionManager::cancel();
        updateEffectiveColors();

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

        currentTheme.harmony =
            ColorManager::Harmony::Manual;

        currentTheme.frontAnimation = AnimationManager::Animation();
        currentTheme.rearAnimation = AnimationManager::Animation();
        updateEffectiveColors();

        changed = true;
    }

    // ==================================================
    // Getters
    // ==================================================

    const Theme &getTheme()
    {

        return currentTheme;
    }

    CRGB getEffectiveRearColor()
    {

        return effectiveRearColor;
    }

    // ==================================================
    // Setters
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

    void setRearColor(CRGB color)
    {

        if (currentTheme.rearColor == color)
        {
            return;
        }

        currentTheme.rearColor = color;

        notifyChange();
    }

    void setBrightness(uint8_t brightness)
    {

        if (brightness > Config::MAX_BRIGHTNESS)
        {

            brightness =
                Config::MAX_BRIGHTNESS;
        }

        if (
            currentTheme.brightness == brightness)
        {
            return;
        }

        currentTheme.brightness = brightness;

        notifyChange();
    }

    void setHarmony(
        ColorManager::Harmony harmony)
    {

        if (currentTheme.harmony == harmony)
        {
            return;
        }

        currentTheme.harmony = harmony;

        notifyChange();
    }

    // ==================================================
    // Apply complete theme
    // ==================================================

    void setTheme(const Theme &theme)
    {

        Theme nextTheme = theme;
        if (nextTheme.frontAnimation.effect == AnimationManager::Effect::Rainbow ||
            nextTheme.frontAnimation.effect == AnimationManager::Effect::Chase)
            nextTheme.frontAnimation.effect = AnimationManager::Effect::Solid;
        if (nextTheme.frontAnimation.speed > 100) nextTheme.frontAnimation.speed = 100;
        if (nextTheme.rearAnimation.speed > 100) nextTheme.rearAnimation.speed = 100;

        if (
            nextTheme.brightness >
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
                nextTheme.brightness &&

            currentTheme.harmony ==
                nextTheme.harmony &&
            currentTheme.frontAnimation.effect == nextTheme.frontAnimation.effect &&
            currentTheme.frontAnimation.speed == nextTheme.frontAnimation.speed &&
            currentTheme.rearAnimation.effect == nextTheme.rearAnimation.effect &&
            currentTheme.rearAnimation.speed == nextTheme.rearAnimation.speed)
        {

            return;
        }

        currentTheme = nextTheme;

        notifyChange();
    }

    // ==================================================
    // Render state
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
