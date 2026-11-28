#include <memory>
#include <new>
#include <time.h>
#include "weather_manager.h"
#include "weather_logic.h"
#include "weather_http_body.h"
#include "storage.h"
#include "time_manager.h"
#include <ESP8266WiFi.h>
#include <stdlib.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <math.h>
namespace WeatherManager {
static Config cfg; static uint32_t last=0,attempt=0,rev=0; static bool pending=true,have=false,retrySoon=false; static int16_t temp=0; static uint8_t weather=0, daylight=0; static const char* state="disabled";
static const char path[]="/config/weather.json", tmp[]="/config/weather.tmp";
static bool sane(const Config& c){return c.lat>=-9000000&&c.lat<=9000000&&c.lon>=-18000000&&c.lon<=18000000&&c.enabled<=1&&c.forecast<=1&&c.zone<=2&&c.intensity<=100;}
static bool read(const char* name,Config& c){File f=LittleFS.open(name,"r");if(!f||f.size()>256)return false;StaticJsonDocument<256> d;if(deserializeJson(d,f))return false; if(!d["lat"].is<int32_t>()||!d["lon"].is<int32_t>())return false;for(const char* k:{"enabled","forecast","zone","intensity"})if(!d[k].is<uint8_t>())return false;c.lat=d["lat"];c.lon=d["lon"];c.enabled=d["enabled"];c.forecast=d["forecast"];c.zone=d["zone"];c.intensity=d["intensity"];return sane(c);}
void begin(){Config c;if(Storage::isReady()&&read(path,c))cfg=c;state=cfg.enabled?"waiting":"disabled";}
const Config& config(){return cfg;}
bool save(const Config& c){if(!sane(c)||!Storage::isReady())return false;StaticJsonDocument<256> d;d["lat"]=c.lat;d["lon"]=c.lon;d["enabled"]=c.enabled;d["forecast"]=c.forecast;d["zone"]=c.zone;d["intensity"]=c.intensity;File f=LittleFS.open(tmp,"w");if(!f)return false;size_t n=serializeJson(d,f);f.close();Config check;if(!n||!read(tmp,check)||!LittleFS.rename(tmp,path))return false;cfg=c;have=false;pending=true;retrySoon=false;state=c.enabled?"waiting":"disabled";++rev;return true;}
bool reset(){return save(Config());} void refresh(){if(cfg.enabled){pending=true;retrySoon=false;}} uint32_t revision(){return rev;} bool valid(){return cfg.enabled&&have&&WeatherLogic::fresh(millis(),last);} int code(){return weather;} int day(){return daylight;} int temperature(){return temp;} uint32_t age(){return have?(millis()-last)/1000:0;} const char* status(){return state;}
// HTTP/1.0 with close-delimited or Content-Length body; bounded headers and body.
static int nextByte(WiFiClient& c,uint32_t start){
 while(millis()-start<4500){
  if(c.available()){int v=c.read();if(v>=0)return v;}
  if(!c.connected()){
   // connected() can update the socket state; drain any bytes still queued.
   if(c.available()){int v=c.read();if(v>=0)return v;}
   else return -1;
  }
  delay(1);
 }
 return -2;
}
static bool line(WiFiClient& c,char* out,size_t cap,uint32_t start){size_t n=0;while(true){int v=nextByte(c,start);if(v<0)return false;if(v=='\n'){out[n]=0;return true;}if(v!='\r'){if(n+1>=cap)return false;out[n++]=char(v);}}}
static bool receive(char* body,size_t cap,size_t& len){
 WiFiClient c;c.setTimeout(1800);
 if(!c.connect("api.open-meteo.com",80)){state="connect";return false;}
 uint32_t start=millis();
 char header[320];int n=snprintf(header,sizeof(header),"GET /v1/forecast?latitude=%.5f&longitude=%.5f&current=temperature_2m,weather_code,is_day&timeformat=unixtime%s HTTP/1.0\r\nHost: api.open-meteo.com\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n",cfg.lat/100000.0,cfg.lon/100000.0,cfg.forecast?"&hourly=weather_code,is_day&forecast_hours=4":"");
 if(n<=0||size_t(n)>=sizeof(header)||c.write(reinterpret_cast<const uint8_t*>(header),n)!=size_t(n)){state="send";return false;}
 if(!line(c,header,sizeof(header),start)){state="header";return false;}
 if(strncmp(header,"HTTP/1.",7)||strlen(header)<12||strncmp(header+9,"200",3)||(header[12]&&header[12]!=' ')){state="http-code";return false;}
 long length=-1;unsigned total=0;
 while(true){
  if(!line(c,header,sizeof(header),start)){state="header";return false;}
  total+=strlen(header)+2;if(total>2048){state="header-limit";return false;}if(!header[0])break;
  if(!strncasecmp(header,"Content-Length:",15)){
   char* end;const char* value=header+15;while(*value==' '||*value=='\t')++value;
   if(*value<'0'||*value>'9'||length>=0){state="header-length";return false;}
   length=strtol(value,&end,10);while(*end==' '||*end=='\t')++end;
   if(*end||length<0||static_cast<unsigned long>(length)>=cap){state="body-limit";return false;}
  }
  if(!strncasecmp(header,"Transfer-Encoding:",18)){state="encoding";return false;}
  if(!strncasecmp(header,"Content-Encoding:",17)){const char* encoding=header+17;while(*encoding==' '||*encoding=='\t')++encoding;if(strcasecmp(encoding,"identity")){state="encoding";return false;}}
 }
 auto result=WeatherHttp::read([&](){return nextByte(c,start);},body,cap,length,len);c.stop();
 switch(result){
  case WeatherHttp::Result::Ok:return true;
  case WeatherHttp::Result::Empty:state="body-empty";break;
  case WeatherHttp::Result::Incomplete:state="body-incomplete";break;
  case WeatherHttp::Result::Timeout:state="body-timeout";break;
  case WeatherHttp::Result::Limit:state="body-limit";break;
  default:state="body-invalid";break;
 }
 return false;
}
static void logBody(size_t len){Serial.print(F("Weather body: "));Serial.print(state);Serial.print(F(", bytes="));Serial.println(static_cast<unsigned>(len));}
static bool fetch(){
 if(ESP.getFreeHeap()<6500||ESP.getMaxFreeBlockSize()<3500){state="memory";return false;}
 std::unique_ptr<char[]> body(new(std::nothrow) char[1536]);if(!body){state="memory";return false;}size_t len=0;
 if(!receive(body.get(),1536,len)){logBody(len);return false;}
 // Allocate documents only after the TCP client has been destroyed. Avoid a
 // large nested stack frame and zero-copy input mutation during filtered parse.
 DynamicJsonDocument filter(384);DynamicJsonDocument d(768);
 if(filter.capacity()<384||d.capacity()<768){state="json-memory";return false;}
 filter["current"]["weather_code"]=true;filter["current"]["is_day"]=true;filter["current"]["temperature_2m"]=true;filter["current"]["time"]=true;
 if(cfg.forecast){filter["hourly"]["time"][0]=true;filter["hourly"]["weather_code"][0]=true;filter["hourly"]["is_day"][0]=true;}
 auto err=deserializeJson(d,static_cast<const char*>(body.get()),len,DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(6));
 body.reset();
 if(err){switch(err.code()){case DeserializationError::EmptyInput:state="json-empty";break;case DeserializationError::IncompleteInput:state="json-incomplete";break;case DeserializationError::NoMemory:state="json-memory";break;case DeserializationError::TooDeep:state="json-depth";break;default:state="json";break;}logBody(len);return false;}
 JsonObject cur=d["current"];if(!cur["weather_code"].is<int>()||!cur["is_day"].is<int>()||!cur["temperature_2m"].is<float>()||!cur["time"].is<uint32_t>()){state="fields";return false;}int c=cur["weather_code"],dy=cur["is_day"];float t=cur["temperature_2m"];uint32_t stamp=cur["time"];if(TimeManager::isValid()){int64_t delta=int64_t(time(nullptr))-stamp;if(delta>7200||delta< -900){state="timestamp";return false;}}
 if(cfg.forecast){JsonArray times=d["hourly"]["time"],codes=d["hourly"]["weather_code"],days=d["hourly"]["is_day"];bool found=false;for(unsigned i=0;i<times.size()&&i<4;++i){if(times[i].is<uint32_t>()&&times[i].as<uint32_t>()>=stamp+1800&&codes[i].is<int>()&&days[i].is<int>()){c=codes[i];dy=days[i];found=true;break;}}if(!found){state="forecast";return false;}}
 if(!WeatherLogic::codeValid(c)||(dy!=0&&dy!=1)||!isfinite(t)||t< -100||t>70){state="range";return false;}weather=c;daylight=dy;temp=int16_t(lroundf(t*10));last=millis();have=true;state="ok";++rev;return true;}
void update(bool allowed){
 if(have&&!WeatherLogic::fresh(millis(),last)){have=false;++rev;state="expired";}
 if(!cfg.enabled||!allowed||WiFi.status()!=WL_CONNECTED||!TimeManager::isValid())return;
 uint32_t now=millis();
 if(!pending&&now-attempt<(retrySoon?10000UL:(strcmp(state,"ok")==0?1800000UL:300000UL)))return;
 if(attempt&&now-attempt<10000)return;
 bool wasRetry=retrySoon&&!pending;pending=false;retrySoon=false;attempt=now;
 uint32_t before=ESP.getFreeHeap(),started=millis();bool ok=fetch();
 if(!ok&&!wasRetry&&(!strcmp(state,"body-incomplete")||!strcmp(state,"body-timeout")||!strcmp(state,"json-incomplete")))retrySoon=true;
 Serial.print(F("Weather: "));Serial.print(state);Serial.print(F(", ms="));Serial.print(millis()-started);
 Serial.print(F(", heap="));Serial.print(before);Serial.print(F(" -> "));Serial.print(ESP.getFreeHeap());
 Serial.print(F(", block="));Serial.println(ESP.getMaxFreeBlockSize());
}
void apply(ThemeManager::Theme& t,bool allowed){if(!allowed||!valid()||!cfg.intensity)return;static const uint32_t front[]={0xffe8ae,0xe2e8f0,0xa5dcff,0xded1ff,0xffffff,0xdddeda,0xd5dfff};static const uint32_t rear[]={0xed9b29,0x64748b,0x2266df,0x7036ba,0x8dddf7,0x909b9e,0x293d86};unsigned p=WeatherLogic::palette(weather,daylight);uint8_t a=uint16_t(cfg.intensity)*255/100;const CRGB originalRear=ColorManager::resolveRearColor(t.frontColor,t.rearColor,t.harmony);if(cfg.zone!=0){t.frontColor=blend(t.frontColor,CRGB(front[p]),a);t.frontAnimation.effect=AnimationManager::Effect::Solid;}if(cfg.zone==1){t.rearColor=originalRear;t.harmony=ColorManager::Harmony::Manual;}if(cfg.zone!=1){t.rearColor=blend(originalRear,CRGB(rear[p]),a);t.harmony=ColorManager::Harmony::Manual;t.rearAnimation.effect=(p>=2&&p<=5)?AnimationManager::Effect::Breathing:AnimationManager::Effect::Solid;t.rearAnimation.speed=20;}}
}
