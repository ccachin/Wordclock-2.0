#include "weather_logic.h"
#include <cassert>
int main(){for(int i=-1;i<256;i++){if(WeatherLogic::codeValid(i)){assert(WeatherLogic::palette(i,true)<7);assert(WeatherLogic::palette(i,false)<7);}}assert(!WeatherLogic::codeValid(54));assert(WeatherLogic::codeValid(56));assert(WeatherLogic::codeValid(66));assert(WeatherLogic::palette(95,true)==3);assert(WeatherLogic::palette(85,false)==4);assert(WeatherLogic::palette(0,false)==6);assert(WeatherLogic::fresh(10,0xfffffff0));assert(!WeatherLogic::fresh(7200000,0));}
