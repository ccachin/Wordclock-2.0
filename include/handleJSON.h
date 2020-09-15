void readJsonFile() {
Serial.println("Reading JSON file");
const size_t capacity = JSON_ARRAY_SIZE(3) + 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(15) + 580;
StaticJsonDocument <capacity> doc;
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

  frontBrightnessVal = doc[1]["brightness"]["frontBrightnessVal"].as<int>();
  backBrightnessVal = doc[1]["brightness"]["backBrightnessVal"].as<int>();

  JsonObject standby = doc[2]["standby"];
  standByState = standby["standByState"].as<int>();
  startStandByHours = standby["startStandByHours"].as<int>();
  startStandByMinutes = standby["startStandByMinutes"].as<int>();
  endStandByHours = standby["endStandByHours"].as<int>();
  endStandByMinutes = standby["endStandByMinutes"].as<int>();
  frontStandByBrightness = standby["frontStandByBrightness"].as<int>();
  backStandByBrightness = standby["backStandByBrightness"].as<int>();
  file.close();
}

void editJsonFile() {
  Serial.println("Editing JSON file");
  const size_t capacity = JSON_ARRAY_SIZE(3) + 3 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(7) + JSON_OBJECT_SIZE(15) + 580;
  StaticJsonDocument <capacity> doc;

  File file = SPIFFS.open("/datas.jso", "w");
  doc[0]["color"]["frontRedVal"]=frontRedVal;
  doc[0]["color"]["frontGreenVal"]=frontGreenVal;
  doc[0]["color"]["frontBlueVal"]=frontBlueVal;
  doc[0]["color"]["backLeftRedVal"]=backLeftRedVal;
  doc[0]["color"]["backLeftGreenVal"]=backLeftGreenVal;
  doc[0]["color"]["backLeftBlueVal"]=backLeftBlueVal;
  doc[0]["color"]["backRightRedVal"]=backRightRedVal;
  doc[0]["color"]["backRightGreenVal"]=backRightGreenVal;
  doc[0]["color"]["backRightBlueVal"]=backRightBlueVal;
  doc[0]["color"]["backBottomRedVal"]=backBottomRedVal;
  doc[0]["color"]["backBottomGreenVal"]=backBottomGreenVal;
  doc[0]["color"]["backBottomBlueVal"]=backBottomBlueVal;
  doc[0]["color"]["backTopRedVal"]=backTopRedVal;
  doc[0]["color"]["backTopGreenVal"]=backTopGreenVal;
  doc[0]["color"]["backTopBlueVal"]=backTopBlueVal;

  doc[1]["brightness"]["frontdocVal"]=frontBrightnessVal;
  doc[1]["brightness"]["backBrightnessVal"]=frontBrightnessVal;

  doc[2]["standby"]["standByState"]=standByState;
  doc[2]["standby"]["startStandByHours"]=startStandByHours;
  doc[2]["standby"]["startStandByMinutes"]=startStandByMinutes;
  doc[2]["standby"]["endStandByHours"]=endStandByHours;
  doc[2]["standby"]["endStandByMinutes"]=endStandByMinutes;
  doc[2]["standby"]["frontStandByBrightness"]=frontStandByBrightness;
  doc[2]["standby"]["backStandByBrightness"]=backStandByBrightness;

  serializeJsonPretty(doc, file);
  file.close();

}