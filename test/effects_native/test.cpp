#include "front_effects.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
int main() {
    using namespace FrontEffects;
    Settings defaults, result;
    assert(defaults.enabled && !defaults.hourlyColor && !defaults.transition && !defaults.chime && !defaults.spatial);
    assert(unpack(pack(defaults),result)); assert(pack(result)==pack(defaults));
    assert(!unpack(0,result)); assert(!unpack(0xffffffff,result));
    unsigned tested=0;
    for(unsigned kind=0;kind<=Circle;++kind) for(unsigned chime=0;chime<=2;++chime)
    for(unsigned spatial=0;spatial<=2;++spatial) for(unsigned intensity: {0u,60u,100u}) for(unsigned speed: {0u,50u,100u}) {
        if(kind==Cuckoo) continue;
        Settings s; s.enabled=kind%2; s.hourlyColor=chime%2; s.transition=kind;s.chime=chime;s.spatial=spatial;s.intensity=intensity; s.spatialSpeed=speed;
        for(unsigned duration=1000;duration<=5000;duration+=100) {
            s.duration=duration; assert(valid(s)); assert(unpack(pack(s),result)); assert(pack(s)==pack(result)); ++tested;
        }
    }
    Settings bad; bad.duration=1501; assert(!valid(bad)); bad=Settings(); bad.transition=Cuckoo; assert(!valid(bad));
    assert(!chimeDue(2,-1,-1,12,0,0)); assert(!chimeDue(2,11,58,12,0,0));
    assert(chimeDue(1,11,59,12,0,0)); assert(chimeDue(1,23,59,0,0,5));
    assert(!chimeDue(1,12,59,13,0,0)); assert(chimeDue(2,12,59,13,0,0));
    assert(!chimeDue(2,12,59,13,0,6)); assert(!chimeDue(2,13,0,13,0,1));
    assert(!chimeDue(0,11,59,12,0,0));
    assert(progress(0,2500)==0); assert(progress(1250,2500)==127); assert(progress(2500,2500)==255); assert(progress(0xffffffff,2500)==255);
    for(unsigned k=Fade;k<=Circle;++k) {
        unsigned lit=0;
        for(unsigned i=0;i<121;++i) for(unsigned p=0;p<=255;++p) {
            auto a=overlay(static_cast<Kind>(k),i,p); auto b=reveal(static_cast<Kind>(k),i,p);
            if(i>=117 || p==255) assert(a==0 && b==255);
            if(!p) assert(a==0);
            if(k==Fade || k==Reveal || k==Dissolve) assert(a==0); else assert(b==255);
            lit+=a>0 || b<255;
        }
        assert(lit>0);
    }
    std::printf("PASS front effects: %u settings round trips, timing, all effect pixels and minute indicators\n",tested);
}
