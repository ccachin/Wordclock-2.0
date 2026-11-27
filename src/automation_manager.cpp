#include "automation_manager.h"
#include "animation_manager.h"
#include "theme_library.h"
#include "storage.h"
#include "time_manager.h"
#include <LittleFS.h>
#include <time.h>
namespace AutomationManager {
using namespace AutomationRules;
static Rule rules[Count];
static uint32_t fired[Count];
static uint32_t rev=0,lastPoll=0,lastTimeRevision=0;
static int64_t lastEpoch=0;
static unsigned lastMinute=0;
static int8_t activeTheme=-1,activeCap=-1;
static ThemeManager::Theme resolved;
static bool resolvedOk=false;
static const char *path="/config/rules.dat",*tmp="/config/rules.tmp",*bak="/config/rules.bak";
static_assert(sizeof(rules)+sizeof(fired)+sizeof(resolved)<320,"Automation memory budget");
static bool read(const char*name,Rule*out){
    File f=LittleFS.open(name,"r");if(!f||f.size()!=HexLength+4)return false;
    char hex[HexLength+1];uint8_t b[4];
    if(f.read(reinterpret_cast<uint8_t*>(hex),HexLength)!=int(HexLength)||f.read(b,4)!=4)return false;
    uint32_t crc=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);
    return crc==checksum(hex,HexLength)&&fromHex(hex,out);
}
static bool save(const Rule*next){
    if(!Storage::isReady())return false;
    char hex[HexLength+1];toHex(next,hex);uint32_t crc=checksum(hex,HexLength);
    uint8_t b[]={uint8_t(crc),uint8_t(crc>>8),uint8_t(crc>>16),uint8_t(crc>>24)};
    File f=LittleFS.open(tmp,"w");if(!f)return false;
    bool ok=f.write(reinterpret_cast<const uint8_t*>(hex),HexLength)==HexLength&&f.write(b,4)==4;f.close();
    Rule verify[Count];if(!ok||!read(tmp,verify)){LittleFS.remove(tmp);return false;}
    // Never remove the only valid backup if the primary is damaged or absent.
    if(LittleFS.exists(path)){
        if(read(path,verify)){if(LittleFS.exists(bak)&&!LittleFS.remove(bak))return false;if(!LittleFS.rename(path,bak))return false;}
        else if(!LittleFS.remove(path))return false;
    }
    if(!LittleFS.rename(tmp,path))return false;
    return true;
}
void begin(){
    if(!Storage::isReady())return;
    Rule loaded[Count];
    if(read(path,loaded)||read(bak,loaded)) { for(unsigned i=0;i<Count;++i)rules[i]=loaded[i]; return; }
    if(read(tmp,loaded)) {
        // Recover a first save interrupted before activation, without overwriting it.
        if(LittleFS.exists(path)&&!LittleFS.remove(path))return;
        if(!LittleFS.rename(tmp,path))return;
        for(unsigned i=0;i<Count;++i)rules[i]=loaded[i];
    }
}
void encode(char*out){toHex(rules,out);}
uint32_t revision(){return rev;}
static void install(const Rule*next){for(unsigned i=0;i<Count;++i)rules[i]=next[i];activeTheme=activeCap=-1;resolvedOk=false;lastEpoch=0; ++rev;}
bool replace(const char*hex,size_t length){Rule next[Count];if(length!=HexLength||!fromHex(hex,next)||!save(next))return false;install(next);return true;}
bool reset(){Rule next[Count];if(!save(next))return false;install(next);return true;}
void update(uint32_t ms,bool eligible){
    if(ms-lastPoll<1000)return;const uint32_t elapsed=ms-lastPoll;lastPoll=ms;
    time_t epoch=time(nullptr);struct tm t{};
    if(!TimeManager::isValid()||!localtime_r(&epoch,&t)){lastEpoch=0;if(activeTheme!=-1||activeCap!=-1){activeTheme=activeCap=-1;++rev;}return;}
    Date d{uint16_t(t.tm_year+1900),uint8_t(t.tm_mon+1),uint8_t(t.tm_mday),uint8_t(t.tm_wday)};
    unsigned minute=t.tm_hour*60+t.tm_min;int8_t theme=-1,cap=-1;int eventIndex=-1;
    const uint32_t timeRevision=TimeManager::revision();
    const bool boundary=timeRevision==lastTimeRevision&&naturalBoundary(lastEpoch,epoch,elapsed,lastMinute,minute,t.tm_sec);
    for(unsigned i=0;i<Count;++i){const auto&r=rules[i];if(window(r,d,minute)){if(r.action==1)theme=r.value;else cap=r.value;}if(boundary&&event(r,d,minute)&&fired[i]<dayKey(d)){fired[i]=dayKey(d);eventIndex=i;}}
    lastEpoch=epoch;lastMinute=minute;lastTimeRevision=timeRevision;
    if(theme!=activeTheme||cap!=activeCap||(theme>=0&&!resolvedOk&&boundary)){activeCap=cap;if(theme!=activeTheme||(theme>=0&&!resolvedOk&&boundary)){activeTheme=theme;resolvedOk=theme>=0&&ThemeLibrary::resolveFactory(theme,resolved);}++rev;}
    if(eventIndex>=0&&eligible)AnimationManager::startFrontEffect(static_cast<FrontEffects::Kind>(rules[eventIndex].value),ms);
}
void apply(ThemeManager::Theme& theme,bool eligible){if(!eligible||!theme.brightness)return;if(activeTheme>=0&&resolvedOk){const auto brightness=theme.brightness,front=theme.frontBrightness,rear=theme.rearBrightness;theme=resolved;theme.brightness=brightness;theme.frontBrightness=front;theme.rearBrightness=rear;}if(activeCap>=0){uint8_t cap=unsigned(activeCap)*255/100;if(theme.brightness>cap)theme.brightness=cap;}}
}
