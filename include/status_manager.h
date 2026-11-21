#pragma once

#include <Arduino.h>

namespace StatusManager
{

    enum class State
    {
        Starting,
        WifiConnecting,
        WifiSetupRequired,
        TimeSyncPending,
        Connected,
        Ready
    };

    void begin();
    void setState(State state);
    State getState();
    void update();

}