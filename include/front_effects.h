#pragma once
#include <stdint.h>
namespace FrontEffects {
// One optional packed settings value; old firmware can ignore it.
enum Kind : uint8_t { None, Fade, Sweep, Matrix, Reveal, Burst, Cuckoo, Dissolve, Diagonal, Scanner, Sparkle, Circle };
struct Settings {
    bool enabled = true, hourlyColor = false;
    uint8_t transition = None, chime = 0, spatial = 0, intensity = 60, spatialSpeed = 50;
    uint16_t duration = 2500;
};
inline bool valid(const Settings& s) {
    return (s.transition <= Burst || (s.transition >= Dissolve && s.transition <= Circle)) && s.chime <= 2 && s.spatial <= 2 && s.intensity <= 100 && s.spatialSpeed <= 100 &&
        s.duration >= 1000 && s.duration <= 5000 && s.duration % 100 == 0;
}
inline uint32_t pack(const Settings& s) {
    return uint32_t(s.enabled) | (uint32_t(s.hourlyColor)<<1) | (uint32_t(s.transition&7)<<2) | (uint32_t(s.transition>>3)<<29) |
        (uint32_t(s.chime)<<5) | (uint32_t(s.spatial)<<7) | (uint32_t(s.intensity)<<9) | (uint32_t(s.duration/100)<<16) | (uint32_t(s.spatialSpeed)<<22);
}
inline bool unpack(uint32_t n, Settings& out) {
    if (n >> 30) return false;
    Settings s; s.enabled=n&1; s.hourlyColor=(n>>1)&1; s.transition=((n>>2)&7)|(((n>>29)&1)<<3);
    s.chime=(n>>5)&3; s.spatial=(n>>7)&3; s.intensity=(n>>9)&127; s.duration=((n>>16)&63)*100; s.spatialSpeed=(n>>22)&127;
    if (!valid(s)) return false;
    out=s; return true;
}
inline bool chimeDue(uint8_t mode, int oldHour, int oldMinute, uint8_t hour, uint8_t minute, uint8_t second) {
    return mode && oldHour >= 0 && oldMinute == 59 && oldHour != hour && minute == 0 && second <= 5 &&
        (mode == 2 || hour == 0 || hour == 12);
}
inline uint8_t progress(uint32_t elapsed, uint16_t duration) {
    return duration && elapsed < duration ? uint8_t(elapsed*255UL/duration) : 255;
}
inline unsigned distance(int a, int b) { return a > b ? a-b : b-a; }
inline uint8_t wave(unsigned delta, unsigned width) { return delta>=width ? 0 : (width-delta)*255/width; }
// Coordinates are logical front matrix indices. Minute dots are excluded by caller.
inline uint8_t ease(uint8_t p) { return uint8_t(uint32_t(p)*p*(765U-2U*p)/65025UL); }
inline unsigned hashPixel(unsigned i) { return (i*73U+19U)%117U; }
inline uint8_t overlay(Kind kind, uint16_t index, uint8_t p) {
    if (p==255 || index>=117) return 0;
    const unsigned x=index%11, y=index/11;
    const unsigned r=distance(x,5)+distance(y,5);
    unsigned level=0;
    switch(kind) {
        case Diagonal: level=wave(distance((x+y)*12,p),42); break;
        case Scanner: level=wave(distance(x*24,p<128?p*2:(255-p)*2),34); break;
        case Sparkle: level=wave(distance((hashPixel(index)*17U)%256U,p),18); break;
        case Circle: level=wave(distance((distance(x,5)*distance(x,5)+distance(y,5)*distance(y,5))*5,p),48); break;
        case Sweep: level=wave(distance(x*24, p),48); break;
        case Matrix: {
            const unsigned head=(unsigned(p)*13/256 + x*7)%14;
            level= head>=y && head-y<3 ? (3-(head-y))*85 : 0; break;
        }
        case Burst: level=wave(distance(r*24,p),48); break;
        case Cuckoo: {
            const unsigned pulse=(unsigned(p)*3)%256;
            const unsigned fade=pulse<32 ? pulse*255/32 : pulse>223 ? (255-pulse)*255/32 : 255;
            level=wave(distance(r*24,pulse),52)*fade/255; break;
        }
        default: break;
    }
    // Gentle envelope at both ends, no abrupt on/off flashes.
    unsigned envelope=p<32 ? p*255/32 : p>223 ? (255-p)*255/32 : 255;
    return level*ease(envelope)/255;
}
inline uint8_t reveal(Kind kind, uint16_t index, uint8_t p) {
    if(p==255 || index>=117) return 255;
    if(kind==Fade) return ease(p);
    if(kind==Dissolve) { const int d=int(p)*141/255-int(hashPixel(index)); return d<=0?0:d>=24?255:ease(d*255/24); }
    if(kind==Reveal) {
        const int d=int(p)* (117+24)/255 - int(index);
        return d<=0 ? 0 : d>=24 ? 255 : ease(d*255/24);
    }
    return 255;
}
}
