#include "time_manager.h"
#include <time.h>
#include <sys/time.h>

namespace TimeManager
{

    // Swiss timezone with automatic daylight saving
    constexpr char TIMEZONE[] =
        "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";

    constexpr char NTP_SERVER_1[] = "ch.pool.ntp.org";
    constexpr char NTP_SERVER_2[] = "pool.ntp.org";

    // January 1st, 2020
    constexpr time_t MIN_VALID_TIME = 1577836800;

    void begin()
    {

        setenv("TZ", TIMEZONE, 1);
        tzset();
    }

    void startNtp()
    {

        configTime(
            TIMEZONE,
            NTP_SERVER_1,
            NTP_SERVER_2);
    }

    bool isValid()
    {

        return time(nullptr) >= MIN_VALID_TIME;
    }

    bool getTime(ClockTime &result)
    {

        if (!isValid())
        {
            return false;
        }

        time_t now = time(nullptr);
        struct tm localTime;

        if (localtime_r(&now, &localTime) == nullptr)
        {
            return false;
        }

        result.hour = localTime.tm_hour;
        result.minute = localTime.tm_min;
        result.second = localTime.tm_sec;

        return true;
    }

    bool setTime(
        uint16_t year,
        uint8_t month,
        uint8_t day,
        uint8_t hour,
        uint8_t minute,
        uint8_t second)
    {

        if (year < 2020 || year > 2099 ||
            month < 1 || month > 12 ||
            day < 1 || day > 31 ||
            hour > 23 ||
            minute > 59 ||
            second > 59)
        {

            return false;
        }

        struct tm newTime = {};

        newTime.tm_year = year - 1900;
        newTime.tm_mon = month - 1;
        newTime.tm_mday = day;
        newTime.tm_hour = hour;
        newTime.tm_min = minute;
        newTime.tm_sec = second;
        newTime.tm_isdst = -1;

        time_t timestamp = mktime(&newTime);

        if (timestamp < MIN_VALID_TIME)
        {
            return false;
        }

        // Reject invalid or normalized dates
        if (newTime.tm_year != year - 1900 ||
            newTime.tm_mon != month - 1 ||
            newTime.tm_mday != day ||
            newTime.tm_hour != hour ||
            newTime.tm_min != minute ||
            newTime.tm_sec != second)
        {

            return false;
        }

        struct timeval tv = {};

        tv.tv_sec = timestamp;
        tv.tv_usec = 0;

        return settimeofday(&tv, nullptr) == 0;
    }

}