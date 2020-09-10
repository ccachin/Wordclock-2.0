void standByBrightness(){
  // Manage standby brightness

  if ((standByState == 1) && (standByFlag == false) && (hours >= startStandByHours) && (hours <= endStandByHours) && (minutes >= startStandByMinutes) && (minutes < endStandByMinutes))
  {
    Serial.println("Entrée en veille");
    previousFrontBrightnessVal = frontBrightnessVal; //Store previous brightness value to restore after standby
    previousBackBrightnessVal = backBrightnessVal;
    frontBrightnessVal = frontStandByBrightness;
    backBrightnessVal = backStandByBrightness;
    standByTime = true;
    standByFlag = true; // pour n'exécuter qu'une fois
  }
  else
  {
    standByTime = false;
  }

  if ((standByState == 1)&&(standByFlag == true) && (standByTime == false))
  {
    Serial.println("Sortie de veille");
    frontBrightnessVal = previousFrontBrightnessVal;
    backBrightnessVal = previousBackBrightnessVal;
    standByFlag = false;
  }
}
void waitAnimation()
{
    int i = 6;
    leds[0].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //IL
    leds[1].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    leds[3].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //EST
    leds[4].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    leds[5].setRGB(frontRedVal, frontGreenVal, frontBlueVal);

    for (i = 6; i <= 121; i++)
    {
        leds[i].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        FastLED.show();
        leds[i].setRGB(0, 0, 0);
        delay(20);
    }
    if (i == 120)
    {
        i = 6;
    }
}
void displayBackPixels(
    int backLeftRedVal, int backLeftGreenVal, int backLeftBlueVal,
    int backRightRedVal, int backRightGreenVal, int backRightBlueVal,
    int backBottomRedVal, int backBottomGreenVal, int backBottomBlueVal,
    int backTopRedVal, int backTopGreenVal, int backTopBlueVal)
{
    // Separate backleds in four segments
    //Bottom frame
    leds[121].setRGB(backBottomRedVal, backBottomGreenVal, backBottomBlueVal);
    leds[122].setRGB(backBottomRedVal, backBottomGreenVal, backBottomBlueVal);
    leds[123].setRGB(backBottomRedVal, backBottomGreenVal, backBottomBlueVal);
    leds[124].setRGB(backBottomRedVal, backBottomGreenVal, backBottomBlueVal);
    leds[125].setRGB(backBottomRedVal, backBottomGreenVal, backBottomBlueVal);
    //Left frame
    leds[126].setRGB(backLeftRedVal, backLeftGreenVal, backLeftBlueVal);
    leds[127].setRGB(backLeftRedVal, backLeftGreenVal, backLeftBlueVal);
    leds[128].setRGB(backLeftRedVal, backLeftGreenVal, backLeftBlueVal);
    leds[129].setRGB(backLeftRedVal, backLeftGreenVal, backLeftBlueVal);
    leds[130].setRGB(backLeftRedVal, backLeftGreenVal, backLeftBlueVal);
    //Top frame
    leds[131].setRGB(backTopRedVal, backTopGreenVal, backTopBlueVal);
    leds[132].setRGB(backTopRedVal, backTopGreenVal, backTopBlueVal);
    leds[133].setRGB(backTopRedVal, backTopGreenVal, backTopBlueVal);
    leds[134].setRGB(backTopRedVal, backTopGreenVal, backTopBlueVal);
    leds[135].setRGB(backTopRedVal, backTopGreenVal, backTopBlueVal);
    //Right frame
    leds[136].setRGB(backRightRedVal, backRightGreenVal, backRightBlueVal);
    leds[137].setRGB(backRightRedVal, backRightGreenVal, backRightBlueVal);
    leds[138].setRGB(backRightRedVal, backRightGreenVal, backRightBlueVal);
    leds[139].setRGB(backRightRedVal, backRightGreenVal, backRightBlueVal);
    leds[140].setRGB(backRightRedVal, backRightGreenVal, backRightBlueVal);

    //Set Brightness
    for (int i = 121; i <= 140; i++)
    {
        leds[i].nscale8(backBrightnessVal);
    }
}
void displayPixelsTime(int frontRedVal, int frontGreenVal, int frontBlueVal)
{
    //Refresh display
    for (int i = 0; i <= 120; i++)
    {
        leds[i] = CRGB::Black;
    }

    /*- Display Stars -*/
    unit_minutes = minutes % 10;
    if (unit_minutes == 1 || unit_minutes == 6)
    {
        leds[117].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (unit_minutes == 2 || unit_minutes == 7)
    {
        leds[117].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[118].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (unit_minutes == 3 || unit_minutes == 8)
    {
        leds[117].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[118].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[119].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (unit_minutes == 4 || unit_minutes == 9)
    {
        leds[117].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[118].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[119].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[120].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    /*------------------------------------------*/

    leds[0].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //IL
    leds[1].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    leds[3].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //EST
    leds[4].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    leds[5].setRGB(frontRedVal, frontGreenVal, frontBlueVal);

    /*-- Show HOUR --*/

    /*-- Hide Hours for midday and midnight--*/
    if (displayedHours == 12 || displayedHours == 24 || displayedHours == 0)
    {
        leds[60] = CRGB::Black; //H
        leds[61] = CRGB::Black; //E
        leds[62] = CRGB::Black; //U
        leds[63] = CRGB::Black; //R
        leds[64] = CRGB::Black; //E
        leds[65] = CRGB::Black; //S
    }
    else
    {
        leds[60].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //H
        leds[61].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //E
        leds[62].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //U
        leds[63].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //R
        leds[64].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //E
        if (displayedHours == 1 || displayedHours == 13)
        {
            leds[65].setRGB(0, 0, 0); // Hide S
        }
        else
        {
            leds[65].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Show S}
        }
    }

    //displayedHours
    if (displayedHours == 1 || displayedHours == 13)
    {
        leds[33].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //une
        leds[34].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[35].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[65] = CRGB::Black; // Hide S for HOUR
    }
    if (displayedHours == 2 || displayedHours == 14)
    {
        leds[7].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //deux
        leds[8].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[9].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[10].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 3 || displayedHours == 15)
    {
        leds[49].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //trois
        leds[50].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[51].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[52].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[53].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 4 || displayedHours == 16)
    {
        leds[11].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //quatre
        leds[12].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[13].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[14].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[15].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[16].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 5 || displayedHours == 17)
    {
        leds[18].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //cinq
        leds[19].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[20].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[21].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 6 || displayedHours == 18)
    {
        leds[22].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //six
        leds[23].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[24].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 7 || displayedHours == 19)
    {
        leds[37].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //sept
        leds[38].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[39].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[40].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 8 || displayedHours == 20)
    {
        leds[44].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //huit
        leds[45].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[46].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[47].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 9 || displayedHours == 21)
    {
        leds[25].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //neuf
        leds[26].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[27].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[28].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 10 || displayedHours == 22)
    {
        leds[41].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //dix
        leds[42].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[43].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 11 || displayedHours == 23)
    {
        leds[29].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //onze
        leds[30].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[31].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[32].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 12)
    {
        leds[56].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //midi
        leds[57].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[58].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[59].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (displayedHours == 0 || displayedHours == 24)
    {
        leds[66].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //minuit
        leds[67].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[68].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[69].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[70].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[71].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }

    //MINUTES
    if (minutes >= 5 && minutes < 10)
    {
        leds[105].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Cinq
        leds[106].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[107].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[108].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 10 && minutes < 15)
    {
        leds[88].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Dix
        leds[89].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[90].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 15 && minutes < 20)
    {
        leds[73].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Et
        leds[74].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[99].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Quart
        leds[100].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[101].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[102].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[103].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 20 && minutes < 25)
    {
        leds[83].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Vingt
        leds[84].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[85].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[86].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[87].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 25 && minutes < 30)
    {
        leds[83].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Vingt
        leds[84].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[85].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[86].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[87].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[105].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Cinq
        leds[106].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[107].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[108].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 30 && minutes < 35)
    {
        leds[73].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Et
        leds[74].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[95].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Demi
        leds[96].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[97].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[98].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 35 && minutes < 40)
    {
        leds[77].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Moins
        leds[78].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[79].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[80].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[81].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[83].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Vingt
        leds[84].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[85].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[86].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[87].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[105].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Cinq
        leds[106].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[107].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[108].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 40 && minutes < 45)
    {
        leds[77].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Moins
        leds[78].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[79].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[80].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[81].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[83].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Vingt
        leds[84].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[85].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[86].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[87].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 45 && minutes < 50)
    {
        leds[77].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Moins
        leds[78].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[79].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[80].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[81].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[99].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Quart
        leds[100].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[101].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[102].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[103].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 50 && minutes < 55)
    {
        leds[77].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Moins
        leds[78].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[79].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[80].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[81].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[88].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Dix
        leds[89].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[90].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }
    if (minutes >= 55 && minutes <= 59)
    {
        leds[77].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Moins
        leds[78].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[79].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[80].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[81].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[105].setRGB(frontRedVal, frontGreenVal, frontBlueVal); //Cinq
        leds[106].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[107].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
        leds[108].setRGB(frontRedVal, frontGreenVal, frontBlueVal);
    }

    for (int i = 0; i <= 120; i++)
    {
        leds[i].nscale8(frontBrightnessVal);
    }
}