#include "time_manager.h"
#include <cassert>
#include <ctime>
#include <cstring>
#include <functional>
#include <sys/time.h>
static time_t clockEpoch = 0;
static std::function<void(bool)> callback;
void settimeofday_cb(std::function<void(bool)> cb) { callback = cb; }
extern "C" time_t __wrap_time(time_t* out) { if (out) *out=clockEpoch; return clockEpoch; }
extern "C" int __wrap_settimeofday(const timeval* value, const struct timezone*) { clockEpoch=value->tv_sec; if(callback) callback(false); return 0; }
int main() {
    using namespace TimeManager;
    begin(); assert(!isValid()); assert(!strcmp(source(),"unknown"));
    assert(setTimezone("Europe/Zurich")); assert(!setTimezone("invalid"));
    assert(!setEpoch(0)); assert(!setEpoch(4102444800ULL));
    assert(!setTime(2026,2,29,12,0,0)); assert(!setTime(2026,4,31,12,0,0));
    assert(!setTime(2026,3,29,2,30,0)); // nonexistent spring DST hour
    assert(setTime(2028,2,29,12,34,56));
    char date[24]; assert(localDateTime(date,sizeof(date))); assert(!strcmp(date,"2028-02-29T12:34:56"));
    assert(!strcmp(source(),"manual"));
    assert(setEpoch(1767225600ULL)); // 2026-01-01 UTC midnight
    ClockTime t{}; assert(getTime(t)); assert(t.hour==1 && t.minute==0);
    ++clockEpoch; assert(getTime(t)&&t.second==1);
    callback(true); assert(!strcmp(source(),"ntp"));
    assert(setTimezone("Etc/UTC")); assert(getTime(t)&&t.hour==0);
    assert(setEpoch(1767225600ULL)); assert(!strcmp(source(),"manual"));
}
