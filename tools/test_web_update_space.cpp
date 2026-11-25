#include "web_update_space.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
    using namespace WebUpdateSpace;
    auto a = estimate(1024000,311296,149606);
    assert(a.sufficient && a.freeBytes == 712704 && a.requiredBytes == 354406 && !a.error);
    assert(estimate(a.requiredBytes,0,149606).sufficient);
    assert(!estimate(a.requiredBytes-1,0,149606).sufficient);
    assert(!estimate(0,0,149606).sufficient);
    assert(!estimate(10,11,149606).sufficient);
    assert(!estimate(1024000,0,0).sufficient);
    assert(!estimate(1024000,0,393217).sufficient);
    assert(estimate(1024000,311296,393216).sufficient);
    assert(!estimate(1024000,1024000,1).sufficient);
    assert(!estimate(1024000,0,0xffffffffu).sufficient);
    puts("10 assertions passed");
}
