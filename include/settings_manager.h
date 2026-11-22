
#pragma once
#include <stdint.h>

namespace SettingsManager
{

    uint32_t faceColor();
    void setFaceColor(uint32_t rgb);
    void begin();
    void update();

    void markChanged();

    bool load();
    bool save();

}
