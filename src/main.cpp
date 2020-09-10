#include <Arduino.h>
#include <ArduinoJson.h>
const size_t capacity = JSON_ARRAY_SIZE(3) + 3*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(15) + 580;
DynamicJsonDocument doc(capacity);
const char* json = "[{\"color\":{\"frontRedVal\":\"128\",\"frontGreenVal\":\"128\",\"frontBlueVal\":\"128\",\"backLeftRedVal\":\"128\",\"backLeftGreenVal\":\"128\",\"backLeftBlueVal\":\"128\",\"backRightRedVal\":\"128\",\"backRightGreenVal\":\"128\",\"backRightBlueVal\":\"128\",\"backBottomRedVal\":\"128\",\"backBottomGreenVal\":\"128\",\"backBottomBlueVal\":\"128\",\"backTopRedVal\":\"128\",\"backTopGreenVal\":\"128\",\"backTopBlueVal\":\"128\"}},{\"luminosity\":{\"frontBrightnessVal\":\"128\",\"backBrightnessVal\":\"128\"}},{\"standby\":{\"standByState\":\"0\",\"startStandByHours\":\"20\",\"startStandByMinutes\":\"00\",\"endStandByHours\":\"07\",\"endStandByMinutes\":\"00\",\"frontStandByBrightness\":\"30\",\"backStandByBrightness\":\"30\"}}]";
deserializeJson(doc, json);

//Manage Server
#include <ESPAsyncWebServer.h>
#include <FS.h>
//#include <LittleFS.h>

//Manage LEDS
#include <FastLED.h>
#define NUM_LEDS 141
#define DATA_PIN 4
CRGBArray<NUM_LEDS> leds;
//CRGB leds[NUM_LEDS];
#define LED_TYPE WS2812
#define COLOR_ORDER BRG

const char *ssid = "Sunrise_2.4GHz_2BC018";
const char *password = "Wk1znMwhM935";

unsigned long previousMillis = 0;

AsyncWebServer server(80);

//Colors from json file
JsonObject root_0_color = doc[0]["color"];
const char* root_0_color_frontRedVal = root_0_color["frontRedVal"]; 
const char* root_0_color_frontGreenVal = root_0_color["frontGreenVal"];
const char* root_0_color_frontBlueVal = root_0_color["frontBlueVal"];
const char* root_0_color_backLeftRedVal = root_0_color["backLeftRedVal"];
const char* root_0_color_backLeftGreenVal = root_0_color["backLeftGreenVal"];
const char* root_0_color_backLeftBlueVal = root_0_color["backLeftBlueVal"];
const char* root_0_color_backRightRedVal = root_0_color["backRightRedVal"];
const char* root_0_color_backRightGreenVal = root_0_color["backRightGreenVal"];
const char* root_0_color_backRightBlueVal = root_0_color["backRightBlueVal"];
const char* root_0_color_backBottomRedVal = root_0_color["backBottomRedVal"];
const char* root_0_color_backBottomGreenVal = root_0_color["backBottomGreenVal"];
const char* root_0_color_backBottomBlueVal = root_0_color["backBottomBlueVal"];
const char* root_0_color_backTopRedVal = root_0_color["backTopRedVal"];
const char* root_0_color_backTopGreenVal = root_0_color["backTopGreenVal"];
const char* root_0_color_backTopBlueVal = root_0_color["backTopBlueVal"];

int frontRedVal = (int)root_0_color_frontRedVal;
int frontGreenVal = (int)root_0_color_frontGreenVal;
int frontBlueVal = (int)root_0_color_frontBlueVal;
int backLeftRedVal = (int)root_0_color_backLeftRedVal;
int backLeftGreenVal = (int)root_0_color_backLeftGreenVal;
int backLeftBlueVal = (int)root_0_color_backLeftBlueVal;

int backRightRedVal = (int)root_0_color_backRightRedVal;
int backRightGreenVal = (int)root_0_color_backRightGreenVal;
int backRightBlueVal = (int)root_0_color_backRightBlueVal;

int backBottomRedVal = (int)root_0_color_backBottomRedVal;
int backBottomGreenVal = (int)root_0_color_backBottomGreenVal;
int backBottomBlueVal = (int)root_0_color_backBottomBlueVal;

int backTopRedVal = (int)root_0_color_backTopRedVal;
int backTopGreenVal = (int)root_0_color_backTopGreenVal;
int backTopBlueVal = (int)root_0_color_backTopBlueVal;


//Stand By parameters from Json file
JsonObject root_2_standby = doc[2]["standby"];
const char* root_2_standby_standByState = root_2_standby["standByState"];
const char* root_2_standby_startStandByHours = root_2_standby["startStandByHours"];
const char* root_2_standby_startStandByMinutes = root_2_standby["startStandByMinutes"];
const char* root_2_standby_endStandByHours = root_2_standby["endStandByHours"];
const char* root_2_standby_endStandByMinutes = root_2_standby["endStandByMinutes"];
const char* root_2_standby_frontStandByBrightness = root_2_standby["frontStandByBrightness"];
const char* root_2_standby_backStandByBrightness = root_2_standby["backStandByBrightness"];

int standByState =(int)root_2_standby_standByState;
int startStandByHours = (int)root_2_standby_startStandByHours;
int startStandByMinutes = (int)root_2_standby_startStandByMinutes;
int endStandByHours = (int)root_2_standby_endStandByHours;
int endStandByMinutes =(int)root_2_standby_endStandByMinutes;
int frontStandByBrightness =(int)root_2_standby_frontStandByBrightness;
int backStandByBrightness =(int)root_2_standby_backStandByBrightness;
bool standByTime;
bool serverFlag = false;
bool standByFlag = false;

const char* root_1_brightness_frontBrightnessVal = doc[1]["brightness"]["frontBrightnessVal"];
const char* root_1_brightness_backBrightnessVal = doc[1]["brightness"]["backBrightnessVal"];

int frontBrightnessVal = (int)root_1_brightness_frontBrightnessVal;
int backBrightnessVal = (int)root_1_brightness_backBrightnessVal;
int previousBackBrightnessVal = 0;
int previousFrontBrightnessVal = 0;


int j = 116;
int ledBuiltIn = 2;
int bootinLed = 0;
bool bootState = HIGH;
int state = HIGH;

//Manage Time
void waitAnimation();
void manageStandBy();
#include "timeManager.h"
#include "displayPixels.h"

void setup()
{
  //-------------------------------------------------------------------------------Serial
  Serial.begin(115200);
  Serial.println("\n");

  //-------------------------------------------------------------------------------GPIO
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(150);
  //-------------------------------------------------------------------------------SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println("Erreur SPIFFS...");
    return;
  }

  //-------------------------------------------------------------------------------WIFI
  WiFi.begin(ssid, password);
  Serial.print("Tentative de connexion...");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }

  Serial.println("\n");
  Serial.println("Connexion etablie!");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());

  //-------------------------------------------------------------------------------SERVER
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/jquery-3.5.1.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/jquery-3.5.1.min.js", "text/javascript");
  });
  server.on("/materialize.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/materialize.min.js", "text/javascript");
  });
  server.on("/jscolor.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/jscolor.js", "text/javascript");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  server.on("/favico.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/favico.ico", "image/x-icon");
  });

  server.on("/materialize.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/materialize.min.css", "text/css");
  });

  //Get values from HTTP request

  //-- Manage colors
  server.on("/setColor", HTTP_POST, [](AsyncWebServerRequest *request) {
    String hexFrontColorVal;
    String hexBackLeftColorVal;
    String hexBackRightColorVal;
    String hexBackBottomColorVal;
    String hexBackTopColorVal;
    if (request->hasParam("frontcolor", true))
    {
      hexFrontColorVal = request->getParam("frontcolor", true)->value();
      uint32_t hexFrontValue = (uint32_t)strtol(hexFrontColorVal.c_str(), NULL, 16);
      frontRedVal = hexFrontValue >> 16;
      frontGreenVal = (hexFrontValue & 0x00ff00) >> 8;
      frontBlueVal = (hexFrontValue & 0x0000ff);
    }
    if (request->hasParam("backLeftColor", true))
    {
      hexBackLeftColorVal = request->getParam("backLeftColor", true)->value();
      uint32_t hexBackLeftValue = (uint32_t)strtol(hexBackLeftColorVal.c_str(), NULL, 16);
      backLeftRedVal = hexBackLeftValue >> 16;
      backLeftGreenVal = (hexBackLeftValue & 0x00ff00) >> 8;
      backLeftBlueVal = (hexBackLeftValue & 0x0000ff);
    }
    if (request->hasParam("backRightColor", true))
    {
      hexBackRightColorVal = request->getParam("backRightColor", true)->value();
      uint32_t hexBackRightValue = (uint32_t)strtol(hexBackRightColorVal.c_str(), NULL, 16);
      backRightRedVal = hexBackRightValue >> 16;
      backRightGreenVal = (hexBackRightValue & 0x00ff00) >> 8;
      backRightBlueVal = (hexBackRightValue & 0x0000ff);
    }
    if (request->hasParam("backBottomColor", true))
    {
      hexBackBottomColorVal = request->getParam("backBottomColor", true)->value();
      uint32_t hexBackBottomValue = (uint32_t)strtol(hexBackBottomColorVal.c_str(), NULL, 16);
      backBottomRedVal = hexBackBottomValue >> 16;
      backBottomGreenVal = (hexBackBottomValue & 0x00ff00) >> 8;
      backBottomBlueVal = (hexBackBottomValue & 0x0000ff);
    }
    if (request->hasParam("backTopColor", true))
    {
      hexBackTopColorVal = request->getParam("backTopColor", true)->value();
      uint32_t hexBackTopValue = (uint32_t)strtol(hexBackTopColorVal.c_str(), NULL, 16);
      backTopRedVal = hexBackTopValue >> 16;
      backTopGreenVal = (hexBackTopValue & 0x00ff00) >> 8;
      backTopBlueVal = (hexBackTopValue & 0x0000ff);
    }

    serverFlag = true;
    request->send(204);
  });

  //---- Manage brightness
  server.on("/setBrightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    String brightness;
    if (request->hasParam("brightnessVal", true))
    {
      brightness = request->getParam("brightnessVal", true)->value();
      frontBrightnessVal = map(brightness.toInt(), 0, 100, 0, 255);
      backBrightnessVal = map(brightness.toInt(), 0, 100, 0, 255);
    }
    serverFlag = true;
    request->send(204);
  });
  server.on("/setFrontBrightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    String frontBrightness;
    if (request->hasParam("frontBrightnessVal", true))
    {
      frontBrightness = request->getParam("frontBrightnessVal", true)->value();
      frontBrightnessVal = map(frontBrightness.toInt(), 0, 100, 0, 255);
    }
    serverFlag = true;
    request->send(204);
  });
  server.on("/setBackBrightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    String backBrightness;
    if (request->hasParam("backBrightnessVal", true))
    {
      backBrightness = request->getParam("backBrightnessVal", true)->value();
      backBrightnessVal = map(backBrightness.toInt(), 0, 100, 0, 255);
    }
    serverFlag = true;
    request->send(204);
  });
  // Manage standBy
  server.on("/activateStandBy", HTTP_POST, [](AsyncWebServerRequest *request) {
    String standByStateReceived;
    if (request->hasParam("standByState", true))
    {
      standByStateReceived = request->getParam("standByState", true)->value();
      standByState = standByStateReceived.toInt();
      Serial.print("standByState: ");
      Serial.println(standByState);
    }
    request->send(204);
  });
  server.on("/setStandBy", HTTP_POST, [](AsyncWebServerRequest *request) {
    String startHoursReceived;
    String startMinutesReceived;
    String endHoursReceived;
    String endMinutesReceived;
    String frontBrightnessReceived;
    String backBrightnessReceived;
    if (request->hasParam("startHoursSet", true))
    {
      startHoursReceived = request->getParam("startHoursSet", true)->value();
      startStandByHours = startHoursReceived.toInt();
      startMinutesReceived = request->getParam("startMinuteSet", true)->value();
      startStandByMinutes = startMinutesReceived.toInt();
      endHoursReceived = request->getParam("endHoursSet", true)->value();
      endStandByHours = endHoursReceived.toInt();
      endMinutesReceived = request->getParam("endMinutesSet", true)->value();
      endStandByMinutes = endMinutesReceived.toInt();
      frontBrightnessReceived = request->getParam("frontBrightnessSet", true)->value();
      frontStandByBrightness = frontBrightnessReceived.toInt();
      backBrightnessReceived = request->getParam("backBrightnessSet", true)->value();
      backStandByBrightness = backBrightnessReceived.toInt();

      Serial.print("Start: ");
      Serial.print(startHoursReceived);
      Serial.print(":");
      Serial.println(startStandByMinutes);
      Serial.print("End: ");
      Serial.print(endHoursReceived);
      Serial.print(":");
      Serial.println(endStandByMinutes);
    }
    request->send(204);
  });
  server.begin();
  Serial.println("Serveur actif!");

  //------------------------------------------------------------------------------- NTP Setup

  configTime(0, 0, NTP_SERVER); // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  lastNTPtime = time(&now);
  lastEntryTime = millis();
}
void loop()
{
  //Get time every 10 seconds
  if (millis() - previousMillis >= 10000)
  {
    getTime();
    previousMillis = millis();
  }

  standByBrightness();

  if (previousMinutes != minutes || serverFlag)
  {
    displayPixelsTime(frontRedVal, frontGreenVal, frontBlueVal);
    displayBackPixels(backLeftRedVal, backLeftGreenVal, backLeftBlueVal,
                      backRightRedVal, backRightGreenVal, backRightBlueVal,
                      backBottomRedVal, backBottomGreenVal, backBottomBlueVal,
                      backTopRedVal, backTopGreenVal, backTopBlueVal);

    previousMinutes = minutes;
    serverFlag = false;
    FastLED.show();
  }
}