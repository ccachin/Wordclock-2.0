#pragma once
#include <Arduino.h>
namespace TransitionManager {
    constexpr uint16_t DEFAULT_DURATION = 1500;
    constexpr uint16_t MAX_DURATION = 5000;
    uint16_t getDuration();
    void setDuration(uint16_t duration);
    void begin();
    void request();
    void cancel();
    bool isActive();
    void apply(uint32_t now, bool timeChanged, uint8_t& brightness);
    void remember(uint8_t brightness);
}
