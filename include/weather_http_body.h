#pragma once
#include <stddef.h>
#include <stdint.h>
namespace WeatherHttp {
enum class Result { Ok, Empty, Incomplete, Timeout, Limit, Invalid };
// Structural framing only: ArduinoJson remains responsible for JSON syntax.
inline Result frame(const char* data, size_t len) {
    char stack[8]; size_t depth=0; bool quoted=false, escape=false, started=false, done=false;
    for(size_t i=0;i<len;++i){
        const char c=data[i];
        if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;else if(static_cast<unsigned char>(c)<32)return Result::Invalid;continue;}
        if(c==' '||c=='\r'||c=='\n'||c=='\t')continue;
        if(done)return Result::Invalid;
        if(!started){if(c!='{')return Result::Invalid;started=true;}
        if(c=='"'){quoted=true;continue;}
        if(c=='{'||c=='['){if(depth==sizeof(stack))return Result::Invalid;stack[depth++]=c;}
        else if(c=='}'||c==']'){if(!depth||stack[depth-1]!=(c=='}'?'{':'['))return Result::Invalid;if(--depth==0)done=true;}
        else if(!depth)return Result::Invalid;
    }
    if(!started)return Result::Empty;
    return done&&!quoted?Result::Ok:Result::Incomplete;
}
// next(): byte 0..255, -1 peer closed, -2 deadline. cap includes terminator.
template<class Next> Result read(Next next, char* out, size_t cap, long length, size_t& len){
    len=0;
    if(!cap||length< -1||(length>=0&&static_cast<unsigned long>(length)>=cap))return Result::Limit;
    while(length<0||len<static_cast<size_t>(length)){
        int v=next();
        if(v==-1){if(length>=0)return Result::Incomplete;break;}
        if(v<0)return Result::Timeout;
        if(len+1>=cap)return Result::Limit;
        if(v==0)return Result::Invalid;
        out[len++]=static_cast<char>(v);
    }
    out[len]=0;return frame(out,len);
}
}
