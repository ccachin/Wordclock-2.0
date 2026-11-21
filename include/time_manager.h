#pragma once

#include <Arduino.h>

namespace TimeManager
{

    struct ClockTime
    {
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
    };

    void begin();
    void startNtp();

    bool isValid();

    bool getTime(ClockTime &result);

    bool setTime(
        uint16_t year,
        uint8_t month,
        uint8_t day,
        uint8_t hour,
        uint8_t minute,
        uint8_t second = 0);

}