#pragma once
#include <stdint.h>
namespace WeatherLogic {
inline bool codeValid(int c){return c==0||c==1||c==2||c==3||c==45||c==48||(c==51||c==53||c==55||c==56||c==57)||(c==61||c==63||c==65||c==66||c==67)||c==71||c==73||c==75||c==77||c==80||c==81||c==82||c==85||c==86||c==95||c==96||c==99;}
inline unsigned palette(int c,bool day){if(c>=95)return 3;if(c==71||c==73||c==75||c==77||c==85||c==86)return 4;if(c==45||c==48)return 5;if(c>=51)return 2;if(c==3||c==2)return 1;return day?0:6;}
inline bool fresh(uint32_t now,uint32_t at){return uint32_t(now-at)<7200000UL;}
}
