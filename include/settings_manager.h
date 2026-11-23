
#pragma once
#include <Arduino.h>

namespace SettingsManager
{

    struct DeviceSettings {
        String name = "Wordclock";
        String timezone = "Europe/Zurich";
        bool setupComplete = false;
    };
    DeviceSettings deviceSettings();
    bool validDeviceSettings(const DeviceSettings &value);
    bool setDeviceSettings(const DeviceSettings &value);
    uint32_t faceColor();
    void setFaceColor(uint32_t rgb);
    void begin();
    void update();

    void markChanged();

    bool load();
    bool save();
    bool savePending();
    bool saveFailed();

}
