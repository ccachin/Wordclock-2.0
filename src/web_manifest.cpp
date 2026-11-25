#include "web_manifest.h"
#include <string.h>

namespace WebManifest {
static const char* const assets[] = {
    "index.html", "style.css", "app.js", "night.js", "studio.js", "theme-rules.js",
    "studio-visual.js", "community.js", "setup.js", "maintenance.js", "icon.svg", "manifest.json"
};
constexpr size_t count = sizeof(assets) / sizeof(assets[0]);
const char* assetName(size_t i) { return i<count ? assets[i] : nullptr; }
int assetIndex(const char* name) { for(size_t i=0;i<count;++i) if(name && !strcmp(name,assets[i])) return i; return -1; }
static bool digit(char c) { return c >= '0' && c <= '9'; }
namespace {
struct Span {
    const char* p = nullptr; size_t n = 0;
    bool equals(const char* s) const { return n==strlen(s) && !memcmp(p,s,n); }
};
// The v1 schema has a fixed shape. Parse directly into Result without a
// second copy, a JSON document, dynamic allocation, or input modification.
class Reader {
    const char* p; const char* end; Result& out;
    void ws() { while(p<end && (*p==' '||*p=='\t'||*p=='\r'||*p=='\n')) ++p; }
    bool take(char c) { ws(); if(p==end||*p!=c)return false; ++p; return true; }
    bool string(Span& s) {
        if(!take('"'))return false;
        s.p=p;
        while(p<end&&*p!='"') { unsigned char c=*p++; if(c<32||c>=127||c=='\\')return false; }
        if(p==end)return false;
        s.n=p-s.p; ++p; return true;
    }
    bool number(uint32_t& n) {
        ws(); n=0;
        if(p==end||!digit(*p))return false;
        if(*p=='0') { ++p; return p==end||!digit(*p); }
        while(p<end&&digit(*p)) {
            unsigned d=*p++-'0';
            if(n>(UINT32_MAX-d)/10)return false;
            n=n*10+d;
        }
        return true;
    }
    bool key(const char* const* names,size_t count,uint32_t& seen,unsigned& i) {
        Span s;
        if(!string(s)||!take(':'))return false;
        for(i=0;i<count;++i)if(s.equals(names[i]))break;
        if(i==count||(seen&(uint32_t(1)<<i)))return false;
        seen|=uint32_t(1)<<i; return true;
    }
    bool version() {
        Span s; if(!string(s)||s.n>32||!s.n)return false;
        const char* q=s.p; const char* e=q+s.n;
        for(unsigned i=0;i<3;++i) {
            if(q==e||!digit(*q)||(*q=='0'&&q+1<e&&digit(q[1])))return false;
            while(q<e&&digit(*q))++q;
            if(i<2&&(q==e||*q++!='.'))return false;
        }
        if(q!=e)return false;
        memcpy(out.version,s.p,s.n); out.version[s.n]=0; return true;
    }
    bool compat() {
        Span s; uint32_t n;
        return take('{')&&string(s)&&s.equals("webApiVersion")&&take(':')&&number(n)&&n==ApiVersion&&take('}');
    }
    static int hex(char c) { return digit(c)?c-'0':c>='a'&&c<='f'?c-'a'+10:-1; }
    bool file(uint32_t& filesSeen,uint32_t& total) {
        static const char* const keys[]={"path","size","sha256"};
        unsigned idx=AssetCount; uint32_t size=0,seen=0; uint8_t hash[32];
        if(!take('{'))return false;
        for(unsigned j=0;j<3;++j) {
            unsigned k;
            if(j&&!take(','))return false;
            if(!key(keys,3,seen,k))return false;
            if(k==0) {
                Span s; if(!string(s))return false;
                for(idx=0;idx<AssetCount;++idx)if(s.equals(assetName(idx)))break;
                if(idx==AssetCount)return false;
            } else if(k==1) {
                if(!number(size)||!size||size>128UL*1024)return false;
            } else {
                Span s; if(!string(s)||s.n!=64)return false;
                for(unsigned i=0;i<32;++i) { int a=hex(s.p[i*2]),b=hex(s.p[i*2+1]); if(a<0||b<0)return false; hash[i]=uint8_t((a<<4)|b); }
            }
        }
        if(!take('}')||seen!=7||idx==AssetCount||(filesSeen&(uint32_t(1)<<idx)))return false;
        if(size>384UL*1024-total)return false;
        total+=size; filesSeen|=uint32_t(1)<<idx;
        out.sizes[idx]=size; memcpy(out.hashes[idx],hash,32); return true;
    }
    bool files(uint32_t& total) {
        uint32_t seen=0;
        if(!take('['))return false;
        for(unsigned i=0;i<AssetCount;++i) {
            if(i&&!take(','))return false;
            if(!file(seen,total))return false;
        }
        return take(']')&&seen==((uint32_t(1)<<AssetCount)-1);
    }
    bool fail(const char* error) { out.error=error; return false; }
public:
    Reader(const char* data,size_t length,Result& r):p(data),end(data+length),out(r){}
    bool read() {
        static const char* const keys[]={"format","manifestVersion","version","compatibility","fileCount","totalBytes","files"};
        uint32_t seen=0,total=0,declared=0,n=0;
        if(!take('{'))return fail("structure");
        for(unsigned j=0;j<7;++j) {
            unsigned k;
            if(j&&!take(','))return fail("structure");
            if(!key(keys,7,seen,k))return fail("strict-json");
            switch(k) {
                case 0: { Span s; if(!string(s)||!s.equals("wordclock-web"))return fail("format"); break; }
                case 1: if(!number(n)||n!=1)return fail("manifest-version"); break;
                case 2: if(!version())return fail("version"); break;
                case 3: if(!compat())return fail("api-version"); break;
                case 4: if(!number(n)||n!=AssetCount)return fail("file-count"); break;
                case 5: if(!number(declared))return fail("total-size"); break;
                case 6: if(!files(total))return fail("files"); break;
            }
        }
        if(!take('}'))return fail("structure");
        ws(); if(p!=end||seen!=127)return fail("strict-json");
        if(total!=declared)return fail("total-size");
        out.fileCount=AssetCount; out.totalBytes=total; return true;
    }
};
}
bool validate(const char* body,size_t length,Result& r) {
    r=Result{};
    if(!body||!length||length>MaxBytes) { r.error="manifest-size"; return false; }
    return Reader(body,length,r).read();
}
bool validateMutable(char* body,size_t length,Result& r) { return validate(body,length,r); }
}
