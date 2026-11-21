#pragma once
#include <Arduino.h>
namespace TransitionManager {
    void begin();
    void request();
    void cancel();
    bool isActive();
    void apply(uint32_t now, bool timeChanged, uint8_t& brightness);
    void remember(uint8_t brightness);
}
