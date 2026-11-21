#include "custom_theme_store.h"
#include "storage.h"
#include <LittleFS.h>
namespace CustomThemeStore {
static const char* DIR="/themes/custom";
static bool valid(const String& id){return id.startsWith("custom-")&&id.length()>7&&ThemeCodec::validId(id);}
static String path(const String& id){return String(DIR)+"/"+id+".json";}
static bool read(const String& p,ThemeCodec::Entry& out){File f=LittleFS.open(p,"r");if(!f||f.size()>ThemeCodec::MAX_BYTES||!f.size())return false;size_t n=f.size();String s=f.readString();f.close();return s.length()==n&&ThemeCodec::decode(s,out);}
bool load(const String& id,ThemeCodec::Entry& out){if(!Storage::isReady()||!valid(id))return false;return read(path(id),out)&&out.id==id;}
size_t count(){if(!Storage::isReady())return 0;Dir d=LittleFS.openDir(DIR);size_t n=0;while(d.next())if(d.fileName().endsWith(".json"))++n;return n;}
static bool write(const ThemeCodec::Entry& e){String s;if(!Storage::isReady()||!valid(e.id)||!ThemeCodec::encode(e,s))return false;String p=path(e.id),tmp=p+".tmp";File f=LittleFS.open(tmp,"w");if(!f)return false;size_t n=f.print(s);f.close();ThemeCodec::Entry check;if(n!=s.length()||!read(tmp,check)||check.id!=e.id){LittleFS.remove(tmp);return false;}if(!LittleFS.rename(tmp,p)){LittleFS.remove(tmp);return false;}return true;}
bool create(ThemeCodec::Entry& e){if(!Storage::isReady()||count()>=MAX_THEMES)return false;ThemeCodec::Entry candidate=e;for(unsigned i=0;i<32;++i){char id[32];snprintf(id,sizeof(id),"custom-%08lx-%08lx",(unsigned long)ESP.getChipId(),(unsigned long)random(0x7fffffff));candidate.id=id;if(!LittleFS.exists(path(candidate.id))){if(!write(candidate))return false;e=candidate;return true;}}return false;}
bool update(const ThemeCodec::Entry& e){return Storage::isReady()&&valid(e.id)&&LittleFS.exists(path(e.id))&&write(e);}
bool remove(const String& id){return Storage::isReady()&&valid(id)&&LittleFS.remove(path(id));}
void runDiagnostics(){
    const String sample=R"({"version":2,"id":"ocean","name":"Ocean","author":"Wordclock","front":"#A9EBFF","rear":"#267DD9","frontEffect":"solid","rearEffect":"breathing","frontSpeed":50,"rearSpeed":35})";
    ThemeCodec::Entry e;String s;bool ok=ThemeCodec::decode(sample,e)&&ThemeCodec::encode(e,s)&&ThemeCodec::decode(s,e)&&e.name=="Ocean"&&e.theme.rearAnimation.speed==35;
    Serial.printf("Theme codec roundtrip: %s\n",ok?"OK":"FAIL");
    String bad=sample;bad.replace("#A9EBFF","#Z9EBFF");Serial.printf("Theme codec invalid color: %s\n",!ThemeCodec::decode(bad,e)?"OK":"FAIL");
    Serial.printf("Theme codec unsafe ID: %s\n",!ThemeCodec::validId("../ocean")?"OK":"FAIL");
    Serial.printf("Custom theme count: %u\n",(unsigned)count());
}
}
