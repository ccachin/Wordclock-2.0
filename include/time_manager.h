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
    bool supportsTimezone(const String &zone);
    bool setTimezone(const String &zone);

    bool isValid();
    uint32_t revision();
    const char* source();
    bool setEpoch(uint64_t epoch);
    bool localDateTime(char* out, size_t size);

    bool getTime(ClockTime &result);

    bool setTime(
        uint16_t year,
        uint8_t month,
        uint8_t day,
        uint8_t hour,
        uint8_t minute,
        uint8_t second = 0);

}