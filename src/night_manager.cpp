#include <Arduino.h>
#include "night_manager.h"
#include "settings_manager.h"
namespace NightManager {
    static Settings settings;
    const Settings& getSettings() { return settings; }
    static uint32_t revision = 0;
    uint32_t getRevision() { return revision; }
    bool equals(const Settings& a, const Settings& b) {
        return a.enabled == b.enabled && a.startMinute == b.startMinute &&
            a.endMinute == b.endMinute && a.brightness == b.brightness && a.mode == b.mode;
    }
    bool isValid(const Settings& value) {
        return value.startMinute < 1440 && value.endMinute < 1440 &&
            value.brightness > 0 && static_cast<uint8_t>(value.mode) <= 2;
    }
    bool setSettings(const Settings& value) {
        if (!isValid(value)) return false;
        if (!equals(settings, value)) {
            settings = value;
            ++revision;
            SettingsManager::markChanged();
        }
        return true;
    }
    static bool inWindow(uint16_t now, uint16_t start, uint16_t end) {
        if (start == end) return false;
        if (start < end) return now >= start && now < end;
        return now >= start || now < end;
    }
    bool isActive(bool valid, uint8_t hour, uint8_t minute) {
        return settings.enabled && valid && hour < 24 && minute < 60 &&
            inWindow(hour * 60 + minute, settings.startMinute, settings.endMinute);
    }
    bool runDiagnostics() {
        bool ok = !inWindow(1349,1350,420) && inWindow(1350,1350,420) &&
            inWindow(0,1350,420) && inWindow(419,1350,420) && !inWindow(420,1350,420) &&
            inWindow(600,600,720) && !inWindow(720,600,720) && !inWindow(600,600,600);
        Serial.println(ok ? "Night mode diagnostics: PASS" : "Night mode diagnostics: FAIL");
        return ok;
    }
}
