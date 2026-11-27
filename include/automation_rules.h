#pragma once
#include <stdint.h>
#include <stddef.h>
namespace AutomationRules {
constexpr uint8_t Count=16;
// Calendar filters: Sunday bit 0; zero month/day means any.
// action 0 disabled, 1 factory theme, 2 brightness cap (%), 3 animation.
struct Rule { uint16_t start=0,end=0; uint8_t days=127,month=0,day=0,action=0,value=0; };
constexpr size_t Bytes=9, HexLength=Count*Bytes*2;
struct Date { uint16_t year; uint8_t month,day,weekday; };
inline bool leap(unsigned y){return y%4==0&&(y%100!=0||y%400==0);}
inline unsigned daysIn(unsigned y,unsigned m){return m==2?28+leap(y):m==4||m==6||m==9||m==11?30:31;}
inline uint32_t dayKey(Date d){return uint32_t(d.year)*512+unsigned(d.month)*32+d.day;}
inline Date previous(Date d){d.weekday=(d.weekday+6)%7;if(d.day>1)--d.day;else{if(d.month>1)--d.month;else{d.month=12;--d.year;}d.day=daysIn(d.year,d.month);}return d;}
inline bool valid(const Rule&r){return r.start<1440&&r.end<1440&&r.days>0&&r.days<=127&&r.month<=12&&r.day<=31&&(!r.month||!r.day||r.day<=daysIn(2024,r.month))&&r.action<=3&& (r.action==0||(r.action==1&&r.value<9&&r.start!=r.end)||(r.action==2&&r.value<=100&&r.start!=r.end)||(r.action==3&&r.value>=1&&r.value<=6&&r.start==r.end));}
inline bool matches(const Rule&r,Date d){return (r.days&(1u<<d.weekday))&&(!r.month||r.month==d.month)&&(!r.day||r.day==d.day);}
inline bool window(const Rule&r,Date d,unsigned minute){if(r.action!=1&&r.action!=2)return false;if(r.start<r.end)return minute>=r.start&&minute<r.end&&matches(r,d);if(minute>=r.start)return matches(r,d);return minute<r.end&&matches(r,previous(d));}
inline bool event(const Rule&r,Date d,unsigned minute){return r.action==3&&r.start==minute&&matches(r,d);}
inline void encode(const Rule&r,uint8_t*b){b[0]=r.start&255;b[1]=r.start>>8;b[2]=r.end&255;b[3]=r.end>>8;b[4]=r.days;b[5]=r.month;b[6]=r.day;b[7]=r.action;b[8]=r.value;}
inline bool decode(const uint8_t*b,Rule&r){r.start=uint16_t(b[0])|(uint16_t(b[1])<<8);r.end=uint16_t(b[2])|(uint16_t(b[3])<<8);r.days=b[4];r.month=b[5];r.day=b[6];r.action=b[7];r.value=b[8];return valid(r);}
inline int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
inline bool fromHex(const char*p,Rule*out){for(unsigned i=0;i<Count;++i){uint8_t b[Bytes];for(unsigned j=0;j<Bytes;++j){int a=nibble(*p++),c=nibble(*p++);if(a<0||c<0)return false;b[j]=a*16+c;}if(!decode(b,out[i]))return false;}return true;}
inline void toHex(const Rule*r,char*out){const char*hex="0123456789abcdef";for(unsigned i=0;i<Count;++i){uint8_t b[Bytes];encode(r[i],b);for(auto c:b){*out++=hex[c>>4];*out++=hex[c&15];}}*out=0;}
inline uint32_t checksum(const char*p,size_t n){uint32_t h=2166136261u;while(n--){h^=uint8_t(*p++);h*=16777619u;}return h;}
// Only a natural minute boundary can trigger an event; skip clock jumps and boot.
inline bool naturalBoundary(int64_t oldEpoch,int64_t epoch,uint32_t elapsedMs,unsigned oldMinute,unsigned minute,unsigned second){return oldEpoch>0&&epoch>oldEpoch&&epoch-oldEpoch<=2&&elapsedMs<=2500&&minute==(oldMinute+1)%1440&&second<=2;}
}
