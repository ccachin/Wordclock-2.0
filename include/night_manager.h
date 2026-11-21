#pragma once
#include <stdint.h>
namespace NightManager {
    enum class Mode : uint8_t { Dim, FrontOnly, Off };
    struct Settings {
        bool enabled = false;
        uint16_t startMinute = 22 * 60 + 30;
        uint16_t endMinute = 7 * 60;
        uint8_t brightness = 13;
        Mode mode = Mode::Dim;
    };
    bool equals(const Settings& a, const Settings& b);
    bool isValid(const Settings& value);
    uint32_t getRevision();
    const Settings& getSettings();
    bool setSettings(const Settings& value);
    bool isActive(bool timeValid, uint8_t hour, uint8_t minute);
    bool runDiagnostics();
}
