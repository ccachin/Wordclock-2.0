#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include "factory_theme_data.h"
struct Record { const char* json; uint32_t front, rear; unsigned speed; };
#define ROW(symbol,id,name,front,rear,effect,label,speed) {WC_FACTORY_THEME_JSON(id,name,front,rear,label,speed),WC_FACTORY_THEME_RGB(front),WC_FACTORY_THEME_RGB(rear),speed},
static const Record rows[]={ WC_FACTORY_THEME_ROWS(ROW) };
#undef ROW
static const char* const expected[]={
    R"json({"version":2,"id":"warm-white","name":"Warm White","author":"Wordclock","front":"#FFE1B4","rear":"#B87838","frontEffect":"solid","rearEffect":"solid","frontSpeed":50,"rearSpeed":50})json",
    R"json({"version":2,"id":"ocean","name":"Ocean","author":"Wordclock","front":"#A9EBFF","rear":"#267DD9","frontEffect":"solid","rearEffect":"solid","frontSpeed":50,"rearSpeed":50})json",
    R"json({"version":2,"id":"sunset","name":"Sunset","author":"Wordclock","front":"#FFB568","rear":"#A63372","frontEffect":"solid","rearEffect":"solid","frontSpeed":50,"rearSpeed":50})json",
    R"json({"version":2,"id":"forest","name":"Forest","author":"Wordclock","front":"#C9E8A5","rear":"#296B48","frontEffect":"solid","rearEffect":"solid","frontSpeed":50,"rearSpeed":50})json",
    R"json({"version":2,"id":"aurora","name":"Aurora","author":"Wordclock","front":"#79F2D0","rear":"#7848D8","frontEffect":"solid","rearEffect":"solid","frontSpeed":50,"rearSpeed":50})json",
    R"json({"version":2,"id":"ocean-pulse","name":"Ocean Pulse","author":"Wordclock","front":"#A9EBFF","rear":"#267DD9","frontEffect":"solid","rearEffect":"breathing","frontSpeed":50,"rearSpeed":70})json",
    R"json({"version":2,"id":"aurora-flow","name":"Aurora Flow","author":"Wordclock","front":"#79F2D0","rear":"#7848D8","frontEffect":"solid","rearEffect":"color-cycle","frontSpeed":50,"rearSpeed":50})json",
    R"json({"version":2,"id":"rainbow","name":"Rainbow","author":"Wordclock","front":"#FFE1B4","rear":"#FFFFFF","frontEffect":"solid","rearEffect":"rainbow","frontSpeed":50,"rearSpeed":65})json",
    R"json({"version":2,"id":"ember","name":"Ember","author":"Wordclock","front":"#FFB568","rear":"#C53D10","frontEffect":"solid","rearEffect":"chase","frontSpeed":50,"rearSpeed":85})json",
};
int main(){
    static_assert(sizeof(rows)/sizeof(rows[0])==9,"theme count");
    for(unsigned i=0;i<9;++i){assert(std::strcmp(rows[i].json,expected[i])==0);assert(std::strlen(rows[i].json)<256);}
    assert(rows[0].front==0xFFE1B4 && rows[0].rear==0xB87838);
    assert(rows[5].speed==70 && rows[7].speed==65 && rows[8].speed==85);
    std::puts("PASS factory catalogue: 9 exact legacy JSON entries");
}
