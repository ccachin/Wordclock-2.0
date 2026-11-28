#ifdef WORDCLOCK_WEATHER_DIAG
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <StackThunk.h>
#include <time.h>
#include <cstring>
#include <strings.h>
#include <cstdlib>
#include "weather_diag.h"
#include "weather_diag_budget.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "web_server.h"
#include "web_update_manager.h"
#include "firmware_update.h"
#include "ota_manager.h"
#include "settings_manager.h"
#if __has_include("weather_diag_ca.local.h")
#include "weather_diag_ca.local.h"
#define WEATHER_DIAG_HAS_CA 1
#endif
namespace WeatherDiag {
namespace {
constexpr size_t sslBytes = sizeof(br_ssl_client_context);
constexpr size_t x509Bytes = sizeof(br_x509_minimal_context);
constexpr size_t clientBytes = sizeof(BearSSL::WiFiClientSecureCtx) + sizeof(BearSSL::WiFiClientSecure);
uint8_t remaining = 0, done = 0;
uint32_t nextAt = 0;
void mem(const char* stage) {
    Serial.printf("WD %s heap=%lu block=%lu frag=%u ms=%lu\n", stage,
        (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMaxFreeBlockSize(),
        (unsigned)ESP.getHeapFragmentation(), (unsigned long)millis());
}
bool budget() {
    using namespace WeatherDiagBudget;
    const bool allocated = stack_thunk_get_refcnt() != 0;
    const size_t heap = ESP.getFreeHeap(), block = ESP.getMaxFreeBlockSize();
    const bool ok = allowed(heap,block,sslBytes,x509Bytes,clientBytes,allocated);
    Serial.printf("WD budget heap=%u block=%u stackRefs=%u ssl=%u x509=%u client=%u minimum=%u reserve=%u required=%u %s\n",
      (unsigned)heap,(unsigned)block,(unsigned)stack_thunk_get_refcnt(),(unsigned)sslBytes,(unsigned)x509Bytes,(unsigned)clientBytes,
      (unsigned)minimum(sslBytes,x509Bytes,clientBytes,allocated),(unsigned)Reserve,
      (unsigned)required(sslBytes,x509Bytes,clientBytes,allocated),ok?"ALLOW":"SKIP_LOW_MEMORY");
    return ok;
}
#ifdef WEATHER_DIAG_HAS_CA
constexpr char Host[] = "api.open-meteo.com";
constexpr char Request[] PROGMEM = "GET /v1/forecast?latitude=46.52&longitude=6.63&current=weather_code,is_day,temperature_2m HTTP/1.0\r\nHost: api.open-meteo.com\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";
// The deadline bounds our polling loop, not DNS/TCP/BearSSL internal calls.
int nextByte(BearSSL::WiFiClientSecure& c, uint32_t start) {
    while (millis()-start < 8000) {
        if (c.available()) return c.read();
        if (!c.connected()) return -1;
        delay(1);
    }
    return -2;
}
bool line(BearSSL::WiFiClientSecure& c, char* out, size_t cap, uint32_t start) {
    size_t n=0; bool overflow=false;
    while (true) {
        const int v=nextByte(c,start);
        if(v<0) return false;
        if(v=='\n') { out[n]=0; return !overflow; }
        if(v=='\r') continue;
        if(n+1<cap) out[n++]=(char)v; else overflow=true;
    }
}
bool readResponse(BearSSL::WiFiClientSecure& c) {
    const uint32_t start=millis();
    char header[256];
    if(!line(c,header,sizeof(header),start) || (strncmp(header,"HTTP/1.0 200 ",13) && strncmp(header,"HTTP/1.1 200 ",13))) {
        Serial.println("WD HTTP status failed"); return false;
    }
    long length=-1; bool end=false;
    for(unsigned i=0;i<40;++i) {
        if(!line(c,header,sizeof(header),start)) return false;
        if(!header[0]) { end=true; break; }
        if(!strncasecmp(header,"Content-Length:",15)) {
            char* tail=nullptr; length=strtol(header+15,&tail,10);
            if(tail==header+15 || *tail || length<1 || length>1024) return false;
        }
        if(!strncasecmp(header,"Transfer-Encoding:",18)) return false;
        if(!strncasecmp(header,"Content-Encoding:",17)) {
            const char* encoding=header+17; while(*encoding==' ') ++encoding;
            if(strcasecmp(encoding,"identity")) return false;
        }
    }
    if(!end) return false;
    char body[1025]; size_t n=0;
    while(n<sizeof(body)-1 && (length<0 || n<(size_t)length)) {
        const int v=nextByte(c,start);
        if(v==-2) return false;
        if(v<0) break;
        body[n++]=(char)v;
    }
    if(length>=0 && n!=(size_t)length) return false;
    if(length<0 && n==sizeof(body)-1) return false;
    body[n]=0;
    const bool ok=strstr(body,"\"weather_code\"") && strstr(body,"\"is_day\"") && strstr(body,"\"temperature_2m\"") && strstr(body,"\"current\"");
    Serial.printf("WD HTTP 200 bytes=%u expected_fields=%s\n",(unsigned)n,ok?"yes":"no");
    return ok;
}
bool tlsAttempt() {
    // CA object precedes client, so remains alive throughout the handshake.
    BearSSL::X509List ca(WeatherDiagRootCA);
    if(ca.getCount()!=1) { Serial.println("WD invalid trust anchor"); return false; }
    mem("CA_READY");
    if(!budget()) return false; // Include actual CA allocation in the decision.
    BearSSL::WiFiClientSecure client;
    mem("CLIENT_CREATED");
    client.setTrustAnchors(&ca);
    client.setX509Time(time(nullptr));
    client.setSSLVersion(BR_TLS12,BR_TLS12);
    client.setBufferSizes(512,512);
    client.setTimeout(5000);
    const uint32_t start=millis();
    const bool connected=client.connect(Host,443);
    mem("AFTER_CONNECT");
    Serial.printf("WD handshake_ms=%lu result=%d\n",(unsigned long)(millis()-start),(int)connected);
    bool ok=false;
    if(!connected) { char error[128]; const int code=client.getLastSSLError(error,sizeof(error)); Serial.printf("WD TLS error=%d %s\n",code,error); }
    else if(!client.getMFLNStatus()) Serial.println("WD FAIL MFLN not negotiated");
    else if(client.write_P(Request,sizeof(Request)-1)==sizeof(Request)-1) { Serial.println("WD authenticated TLS + MFLN OK"); ok=readResponse(client); }
    client.stop();
    mem("STOPPED");
    return ok;
}
#endif
bool attempt() {
    mem("BEFORE");
    if(!budget()) return false;
    if(!WifiManager::isConnected() || WifiManager::isPortalActive() || !TimeManager::isValid()) {
        Serial.println("WD SKIP WiFi or clock not ready"); return false;
    }
#ifndef WEATHER_DIAG_HAS_CA
    Serial.println("WD SKIP_NO_CA: budget-only build; provision CA first"); return false;
#else
    const uint32_t started=millis();
    const bool ok=tlsAttempt();
    mem("RELEASED");
    Serial.printf("WD result=%s synchronous_pause_ms=%lu\n",ok?"PASS":"FAIL",(unsigned long)(millis()-started));
    return ok;
#endif
}
}
void update() {
    while(Serial.available()) {
        const int cmd=Serial.read();
        if(cmd=='b'||cmd=='B') budget();
        else if(cmd=='x'||cmd=='X') { remaining=0; Serial.println("WD cancelled"); }
        else if((cmd=='w'||cmd=='W'||cmd=='t'||cmd=='T') && !remaining) {
            remaining=(cmd=='t'||cmd=='T')?10:1; done=0; nextAt=millis();
        }
    }
    if(!remaining || (int32_t)(millis()-nextAt)<0) return;
    if(WebServer::maintenancePending() || WebServer::firmwareSessionActive() || WebUpdateManager::active() || FirmwareUpdate::active() || OTAManager::isUpdating() || SettingsManager::savePending()) {
        remaining=0; Serial.println("WD SKIP maintenance or settings save"); return;
    }
    Serial.printf("WD attempt=%u\n",(unsigned)done+1);
    const bool ok=attempt();
    if(!ok) { remaining=0; Serial.printf("WD stopped successes=%u\n",(unsigned)done); return; }
    ++done; --remaining; nextAt=millis()+15000;
    if(!remaining) Serial.printf("WD complete successes=%u\n",(unsigned)done);
}
}
#endif
