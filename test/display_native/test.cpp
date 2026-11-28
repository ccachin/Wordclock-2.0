#include "display_state.h"
#include <assert.h>
#include <stdio.h>
using namespace RenderManager;
int main(){
  unsigned count=0;
  for(unsigned flags=0;flags<1024;++flags){
    const bool power=flags&1,night=flags&2,time=flags&4,preview=flags&8,program=flags&16,cap=flags&32,enabled=flags&64,valid=flags&128,intensity=flags&256,maintenance=flags&512;
    const bool eligible=!night&&time&&!preview&&!maintenance;
    const bool programApplied=eligible&&program&&power;
    const bool weatherApplied=eligible&&!program&&enabled&&valid&&intensity;
    const bool capApplied=eligible&&cap;
    for(unsigned zone=0;zone<3;++zone){
      auto d=describeFrame(power,night,time,preview,programApplied,weatherApplied,capApplied,enabled,valid,intensity,zone,maintenance,true);
      assert(d.ready&&d.zone==zone&&d.capped==capApplied);
      assert(d.animated==power);
      if(!power)assert(d.mode==DisplayMode::Off);
      else if(night)assert(d.mode==DisplayMode::Night);
      else if(!time)assert(d.mode==DisplayMode::Waiting);
      else if(preview)assert(d.mode==DisplayMode::Preview);
      else if(programApplied)assert(d.mode==DisplayMode::Program);
      else if(weatherApplied)assert(d.mode==DisplayMode::Weather);
      else if(capApplied)assert(d.mode==DisplayMode::Program);
      else assert(d.mode==DisplayMode::Theme);
      assert((d.weather==0)==!enabled);
      assert((d.weather==1)==(enabled&&d.mode==DisplayMode::Weather));
      if(enabled&&(night||preview||programApplied||!power||!time||maintenance))assert(d.weather==4);
      ++count;
    }
  }
  for(unsigned c=0;c<256;++c)for(unsigned b=0;b<256;++b){
    auto v=previewChannel(c,b);assert(v<=c);assert(v<=b);if(b==0)assert(v==0);if(b==255)assert(v==c);
    assert(hexNibble(v>>4)=="0123456789abcdef"[v>>4]);assert(hexNibble(v&15)=="0123456789abcdef"[v&15]);
  }
  printf("PASS display frame states (%u) and 65536 channel pairs\n",count);
}
