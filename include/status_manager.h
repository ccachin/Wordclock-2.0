
#pragma once

#include <Arduino.h>
#include <FastLED.h>

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

    constexpr uint8_t STATUS_LED_INDEX = 2;

    void begin();
    void update();

    void setState(State state);
    State getState();

    bool isActive();

    CRGB getColor();

}
