#include "time_manager.h"
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
#include <limits>

namespace TimeManager
{

    struct Zone { const char *name; const char *rule; };
    static const Zone zones[] = {
        {"Europe/Zurich", "CET-1CEST-2,M3.5.0/2,M10.5.0/3"},
        {"Europe/Paris", "CET-1CEST-2,M3.5.0/2,M10.5.0/3"},
        {"Europe/Berlin", "CET-1CEST-2,M3.5.0/2,M10.5.0/3"},
        {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
        {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0/2"},
        {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
        {"Etc/UTC", "UTC0"},
        {"America/New_York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
        {"America/Toronto", "EST5EDT,M3.2.0/2,M11.1.0/2"},
        {"America/Los_Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2"},
        {"Asia/Tokyo", "JST-9"}
    };
    static const char *activeRule = zones[0].rule;
    static uint32_t timeRevision=0;
    uint32_t revision(){return timeRevision;}
    bool supportsTimezone(const String &zone) {
        for (const auto &entry : zones) if (zone == entry.name) return true;
        return false;
    }
    bool setTimezone(const String &zone) {
        for (const auto &entry : zones) if (zone == entry.name) {
            if (activeRule != entry.rule) ++timeRevision;
            activeRule = entry.rule;
            setenv("TZ", activeRule, 1);
            tzset();
            return true;
        }
        return false;
    }

    constexpr char NTP_SERVER_1[] = "ch.pool.ntp.org";
    constexpr char NTP_SERVER_2[] = "pool.ntp.org";

    // January 1st, 2020
    constexpr time_t MIN_VALID_TIME = 1577836800;

    static const char* timeSource = "unknown";

    const char* source() { return isValid() ? timeSource : "unknown"; }

    bool localDateTime(char* out, size_t size) {
        if (!isValid()) return false;
        const time_t now = time(nullptr);
        struct tm local{};
        return localtime_r(&now, &local) && strftime(out, size, "%Y-%m-%dT%H:%M:%S", &local) != 0;
    }

    bool setEpoch(uint64_t epoch) {
        if (epoch < MIN_VALID_TIME || epoch > 4102444799ULL || epoch > static_cast<uint64_t>((std::numeric_limits<time_t>::max)())) return false;
        struct timeval tv{};
        tv.tv_sec = static_cast<time_t>(epoch);
        if (settimeofday(&tv, nullptr) != 0) return false;
        ++timeRevision;
        timeSource = "manual";
        return true;
    }

    void begin()
    {

        setenv("TZ", activeRule, 1);
        tzset();
        settimeofday_cb([](bool fromSntp) { if (fromSntp && isValid()) { timeSource = "ntp"; ++timeRevision; } });
    }

    void startNtp()
    {

        configTime(
            activeRule,
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

        return setEpoch(static_cast<uint64_t>(timestamp));
    }

}