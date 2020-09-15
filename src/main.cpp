#include <Arduino.h>
#include <ArduinoJson.h>

//Manage Server
#include <ESPAsyncWebServer.h>
#include <FS.h>


//Manage LEDS
#include <FastLED.h>
#define NUM_LEDS 141
#define DATA_PIN 4
CRGBArray<NUM_LEDS> leds;
//CRGB leds[NUM_LEDS];
#define LED_TYPE WS2812
#define COLOR_ORDER BRG

//Network parameters
const char *ssid = "Sunrise_2.4GHz_2BC018";
const char *password = "Wk1znMwhM935";
AsyncWebServer server(80);


unsigned long previousMillis = 0;


//Colors default values
int frontRedVal = 0;
int frontGreenVal = 0;
int frontBlueVal = 0;
int backLeftRedVal = 0;
int backLeftGreenVal =0;
int backLeftBlueVal = 0;

int backRightRedVal = 0;
int backRightGreenVal = 0;
int backRightBlueVal = 0;

int backBottomRedVal = 0;
int backBottomGreenVal = 0;
int backBottomBlueVal = 0;

int backTopRedVal = 0;
int backTopGreenVal = 0;
int backTopBlueVal = 0;


//Stand By default parameters
int standByState =0;
int startStandByHours = 20;
int startStandByMinutes = 0;
int endStandByHours = 8;
int endStandByMinutes =0;
int frontStandByBrightness =20;
int backStandByBrightness =20;
bool standByTime;
bool serverFlag = false;
bool standByFlag = false;

int frontBrightnessVal = 80;
int backBrightnessVal = 80;
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
#include "handleJSON.h"




void setup()
{
  //-------------------------------------------------------------------------------Serial
  Serial.begin(115200);
  Serial.println("\n");

  //-------------------------------------------------------------------------------GPIO
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  //-------------------------------------------------------------------------------SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println("Erreur SPIFFS...");
    return;
  }
  //-------------------------------------------------------------------------------JSON FILE
  readJsonFile();
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
    editJsonFile();
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
    editJsonFile();
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
    editJsonFile();
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
    editJsonFile();
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
    editJsonFile();
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
    editJsonFile();
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