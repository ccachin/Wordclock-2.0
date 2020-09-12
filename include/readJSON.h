
const size_t capacity = JSON_ARRAY_SIZE(3) + 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(15) + 580;
StaticJsonDocument<capacity> doc;

void readJsonFile() {
  File file = SPIFFS.open("/datas.jso", "r");
     if (!file) {
        Serial.println(F("Failed to read file"));
        return;
    }
  deserializeJson(doc, file);
  
  JsonObject color = doc[0]["color"];
  frontRedVal = color["frontRedVal"].as<int>();
  frontGreenVal = color["frontGreenVal"].as<int>();
  frontBlueVal = color["frontBlueVal"].as<int>();
  backLeftRedVal = color["backLeftRedVal"].as<int>();
  backLeftGreenVal = color["backLeftGreenVal"].as<int>();
  backLeftBlueVal = color["backLeftBlueVal"].as<int>();
  backRightRedVal = color["backRightRedVal"].as<int>();
  backRightGreenVal = color["backRightGreenVal"].as<int>();
  backRightBlueVal = color["backRightBlueVal"].as<int>();
  backBottomRedVal = color["backBottomRedVal"].as<int>();
  backBottomGreenVal = color["backBottomGreenVal"].as<int>();
  backBottomBlueVal = color["backBottomBlueVal"].as<int>();
  backTopRedVal = color["backTopRedVal"].as<int>();
  backTopGreenVal = color["backTopGreenVal"].as<int>();
  backTopBlueVal = color["backTopBlueVal"].as<int>();

  frontBrightnessVal = doc[1]["luminosity"]["frontBrightnessVal"].as<int>();
  backBrightnessVal = doc[1]["luminosity"]["backBrightnessVal"].as<int>();

  JsonObject standby = doc[2]["standby"];
  standByState = standby["standByState"].as<int>();
  startStandByHours = standby["startStandByHours"].as<int>();
  startStandByMinutes = standby["startStandByMinutes"].as<int>();
  endStandByHours = standby["endStandByHours"].as<int>();
  endStandByMinutes = standby["endStandByMinutes"].as<int>();
  frontStandByBrightness = standby["frontStandByBrightness"].as<int>();
  backStandByBrightness = standby["backStandByBrightness"].as<int>();

  Serial.println(frontRedVal);
}