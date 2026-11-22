#include "theme_codec.h"
#include <ArduinoJson.h>
namespace ThemeCodec {
bool validId(const String& id) {
    if (!id.length() || id.length()>48) return false;
    for (size_t i=0;i<id.length();++i) {
        char c=id[i];
        if (!((c>='a'&&c<='z')||(c>='0'&&c<='9')|| (c=='-'&&i>0&&i+1<id.length()))) return false;
    }
    return true;
}
static bool field(JsonVariantConst v, String& out, size_t max, bool required) {
    if (!v.is<const char*>()) return false;
    JsonString s=v.as<JsonString>();
    if (s.size()>max || (required&&!s.size())) return false;
    for(size_t i=0;i<s.size();++i) if ((uint8_t)s.c_str()[i]<32 || s.c_str()[i]==127) return false;
    out=s.c_str(); return out.length()==s.size();
}
static bool color(JsonVariantConst v, CRGB& out) {
    String s; if (!field(v,s,7,true)||s.length()!=7||s[0]!='#') return false;
    uint32_t n=0;
    for(size_t i=1;i<7;++i){char c=s[i]; int x=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; if(x<0)return false; n=(n<<4)|x;}
    out=CRGB(n>>16,(n>>8)&255,n&255);return true;
}
static bool anim(JsonObjectConst obj,const char* effect,const char* speed,AnimationManager::Animation& a,bool front){
    String s; if(!field(obj[effect],s,24,true)||!AnimationManager::effectFromString(s,a.effect))return false;
    if(front&&(a.effect==AnimationManager::Effect::Rainbow||a.effect==AnimationManager::Effect::Chase))return false;
    if(!obj[speed].is<int>()||obj[speed].as<int>()<0||obj[speed].as<int>()>100)return false;
    a.speed=obj[speed].as<int>();return true;
}
bool decode(const String& json, Entry& out, bool allowMissingId){
    if(!json.length()||json.length()>MAX_BYTES)return false;
    DynamicJsonDocument doc(4096);
    if(deserializeJson(doc,json,DeserializationOption::NestingLimit(3))||!doc.is<JsonObject>())return false;
    JsonObjectConst o=doc.as<JsonObjectConst>(); Entry e;
    if(!o["version"].is<int>()||o["version"].as<int>()!=2)return false;
    if (allowMissingId && !o.containsKey("id")) { e.id = "custom-new"; }
    else if (!field(o["id"],e.id,48,true) || !validId(e.id)) return false;
    if (!field(o["name"],e.name,48,true) || !field(o["author"],e.author,48,false)) return false;
    if(o.containsKey("description")&&!field(o["description"],e.description,200,false))return false;
    if(!color(o["front"],e.theme.frontColor)||!color(o["rear"],e.theme.rearColor)||!anim(o,"frontEffect","frontSpeed",e.theme.frontAnimation,true)||!anim(o,"rearEffect","rearSpeed",e.theme.rearAnimation,false))return false;
    if(o.containsKey("harmony")){String h;if(!field(o["harmony"],h,24,true)||!ColorManager::harmonyFromString(h,e.theme.harmony))return false;}
    e.theme.brightness=0;out=e;return true;
}
bool encode(const Entry& e,String& json){
    DynamicJsonDocument doc(2048); char f[8],r[8];
    snprintf(f,sizeof(f),"#%02X%02X%02X",(unsigned)e.theme.frontColor.r,(unsigned)e.theme.frontColor.g,(unsigned)e.theme.frontColor.b);
    snprintf(r,sizeof(r),"#%02X%02X%02X",(unsigned)e.theme.rearColor.r,(unsigned)e.theme.rearColor.g,(unsigned)e.theme.rearColor.b);
    doc["version"]=2;doc["id"]=e.id;doc["name"]=e.name;doc["author"]=e.author;doc["description"]=e.description;doc["front"]=String(f);doc["rear"]=String(r);doc["harmony"]=ColorManager::harmonyToString(e.theme.harmony);doc["frontEffect"]=AnimationManager::effectToString(e.theme.frontAnimation.effect);doc["rearEffect"]=AnimationManager::effectToString(e.theme.rearAnimation.effect);doc["frontSpeed"]=e.theme.frontAnimation.speed;doc["rearSpeed"]=e.theme.rearAnimation.speed;
    if(doc.overflowed())return false;String tmp; if(!serializeJson(doc,tmp))return false;Entry check;if(!decode(tmp,check))return false;json=tmp;return true;
}
}
