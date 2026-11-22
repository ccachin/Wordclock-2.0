#include "preview_manager.h"
#include "wifi_manager.h"
namespace PreviewManager {
    static String owner;
    static uint32_t touched = 0, rev = 0;
    static ThemeManager::Theme temporary;
    bool active() {
        if (owner.length() && (millis()-touched >= TIMEOUT_MS || !WifiManager::isConnected())) {
            owner = ""; ++rev;
        }
        return owner.length() != 0;
    }
    bool set(const String& token, const ThemeManager::Theme& theme) {
        if (active() && owner != token) return false;
        if (!WifiManager::isConnected()) return false;
        if (!owner.length() || temporary.frontColor != theme.frontColor || temporary.rearColor != theme.rearColor || temporary.harmony != theme.harmony || temporary.frontAnimation.effect != theme.frontAnimation.effect || temporary.frontAnimation.speed != theme.frontAnimation.speed || temporary.rearAnimation.effect != theme.rearAnimation.effect || temporary.rearAnimation.speed != theme.rearAnimation.speed) ++rev;
        temporary = theme; owner = token; touched = millis(); return true;
    }
    bool stop(const String& token) {
        if (!active()) return true;
        if (owner != token) return false;
        owner = ""; ++rev; return true;
    }
    uint32_t revision() { active(); return rev; }
    ThemeManager::Theme renderTheme() {
        auto result = active() ? temporary : ThemeManager::getTheme();
        result.brightness = ThemeManager::getTheme().brightness;
        result.frontBrightness = ThemeManager::getTheme().frontBrightness;
        result.rearBrightness = ThemeManager::getTheme().rearBrightness;
        return result;
    }
}
