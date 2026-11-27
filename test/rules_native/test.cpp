#include "automation_rules.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace AutomationRules;
int main(){
 Rule r; r.action=1;r.start=1320;r.end=420;r.days=1<<5;r.month=12;r.day=25;r.value=8;
 Date fri{2026,12,25,5},sat{2026,12,26,6};assert(valid(r));assert(window(r,fri,1320));assert(window(r,sat,419));assert(!window(r,sat,420));assert(!window(r,fri,419));assert(!window(r,sat,1320));
 Date jan{2027,1,1,5};auto p=previous(jan);assert(p.year==2026&&p.month==12&&p.day==31&&p.weekday==4);Date march{2024,3,1,5};assert(previous(march).day==29);assert(daysIn(2100,2)==28);
 r.action=3;r.start=r.end=720;r.value=6;assert(event(r,fri,720));assert(!event(r,sat,720));assert(!event(r,fri,721));
 assert(naturalBoundary(100,101,1000,719,720,0));assert(naturalBoundary(100,102,2000,1439,0,1));assert(!naturalBoundary(0,101,1000,719,720,0));assert(!naturalBoundary(100,160,1000,719,720,0));assert(!naturalBoundary(100,101,1000,719,780,0));assert(!naturalBoundary(100,99,1000,719,720,0));
 Rule rules[Count],out[Count];rules[0]=r;char a[HexLength+1],b[HexLength+1];toHex(rules,a);assert(strlen(a)==HexLength);assert(fromHex(a,out));toHex(out,b);assert(!strcmp(a,b));a[0]='z';assert(!fromHex(a,out));
 unsigned n=0;for(unsigned start=0;start<1440;start+=11)for(unsigned end=0;end<1440;end+=13){Rule q;q.action=2;q.start=start;q.end=end;q.value=100;assert(valid(q)==(start!=end));if(valid(q)){uint8_t data[Bytes];encode(q,data);Rule result;assert(decode(data,result));assert(result.start==start&&result.end==end);++n;}}
 printf("PASS automation rules: %u round trips, calendar, midnight and natural boundaries\n",n);
}
