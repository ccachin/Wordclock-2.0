
#pragma once

namespace SettingsManager
{

    void begin();
    void update();

    void markChanged();

    bool load();
    bool save();

}
