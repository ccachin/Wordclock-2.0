#include "web_manifest.h"
#include <ArduinoJson.h>
#include <string.h>

namespace WebManifest {
static const char* const assets[] = {
    "index.html", "style.css", "app.js", "night.js", "studio.js", "theme-rules.js",
    "studio-visual.js", "community.js", "setup.js", "maintenance.js", "icon.svg", "manifest.json"
};
constexpr size_t count = sizeof(assets) / sizeof(assets[0]);
static bool digit(char c) { return c >= '0' && c <= '9'; }
// Manifest v1 has only ASCII identifiers, hashes, versions and integer values.
// Reject every string escape (including Unicode aliases), and require unique
// literal keys per object. No heap allocation; nesting and keys are bounded.
class StrictJson {
    const char* p;
    const char* end;
    void ws() { while (p<end && (*p==' ' || *p=='\t' || *p=='\r' || *p=='\n')) ++p; }
    bool take(char c) { ws(); if (p==end || *p!=c) return false; ++p; return true; }
    bool str(const char*& start, size_t& len) {
        if (!take('"')) return false;
        start=p;
        while (p<end && *p!='"') {
            unsigned char c=*p++;
            if (c<32 || c>=127 || c=='\\') return false;
        }
        if (p==end) return false;
        len=p-start; ++p; return true;
    }
    bool literal(const char* s) {
        size_t n=strlen(s);
        if (size_t(end-p)<n || memcmp(p,s,n)) return false;
        p+=n; return true;
    }
    bool number() {
        if (p<end && *p=='-') ++p;
        if (p==end || !digit(*p)) return false;
        if (*p=='0') ++p; else while (p<end && digit(*p)) ++p;
        if (p<end && *p=='.') {
            ++p; if (p==end || !digit(*p)) return false;
            while (p<end && digit(*p)) ++p;
        }
        if (p<end && (*p=='e' || *p=='E')) {
            ++p; if (p<end && (*p=='+' || *p=='-')) ++p;
            if (p==end || !digit(*p)) return false;
            while (p<end && digit(*p)) ++p;
        }
        return true;
    }
    bool object(unsigned depth) {
        struct Key { const char* start; size_t len; } keys[7];
        unsigned n=0;
        ++p;
        if (take('}')) return true;
        do {
            const char* start; size_t len;
            if (n==7 || !str(start,len)) return false;
            for (unsigned i=0;i<n;++i)
                if (keys[i].len==len && !memcmp(keys[i].start,start,len)) return false;
            keys[n++]={start,len};
            if (!take(':') || !value(depth+1)) return false;
            if (take('}')) return true;
        } while (take(','));
        return false;
    }
    bool value(unsigned depth) {
        ws(); if (p==end || depth>5) return false;
        switch (*p) {
            case '{': return object(depth);
            case '[':
                ++p; if (take(']')) return true;
                do { if (!value(depth+1)) return false; if (take(']')) return true; } while (take(','));
                return false;
            case '"': { const char* s; size_t n; return str(s,n); }
            case 't': return literal("true");
            case 'f': return literal("false");
            case 'n': return literal("null");
            default: return number();
        }
    }
public:
    StrictJson(const char* data,size_t len):p(data),end(data+len) {}
    bool valid() { if (!value(0)) return false; ws(); return p==end; }
};
static bool versionValid(const char* p) {
    if (!p || strlen(p) > 32) return false;
    for (int i=0; i<3; ++i) {
        if (!digit(*p) || (*p=='0' && digit(p[1]))) return false;
        do { ++p; } while (digit(*p));
        if (i<2 && *p++ != '.') return false;
    }
    return *p=='\0';
}
static bool hashValid(const char* p) {
    if (!p || strlen(p)!=64) return false;
    for (; *p; ++p) if (!digit(*p) && !(*p>='a' && *p<='f')) return false;
    return true;
}
static bool fail(Result& r, const char* error) { r.error=error; return false; }
template<typename Buffer>
static bool validateImpl(Buffer body, size_t length, Result& r) {
    r = Result{};
    if (!body || !length || length>MaxBytes) return fail(r,"manifest-size");
    if (!StrictJson(body,length).valid()) return fail(r,"strict-json");
    // Bounded allocation: the current schema has exactly twelve assets.
    DynamicJsonDocument doc(4096);
    if (!doc.capacity()) return fail(r,"memory");
    const auto err=deserializeJson(doc,body,length,DeserializationOption::NestingLimit(5));
    if (err==DeserializationError::NoMemory) return fail(r,"memory");
    if (err) return fail(r,"json");
    JsonObjectConst root=doc.as<JsonObjectConst>();
    if (root.isNull() || root.size()!=7) return fail(r,"structure");
    const char* format=root["format"].as<const char*>();
    if (!format || strcmp(format,"wordclock-web")) return fail(r,"format");
    if (!root["manifestVersion"].is<uint32_t>() || root["manifestVersion"].as<uint32_t>()!=1) return fail(r,"manifest-version");
    const char* version=root["version"].as<const char*>();
    if (!versionValid(version)) return fail(r,"version");
    JsonObjectConst compat=root["compatibility"].as<JsonObjectConst>();
    if (compat.isNull() || compat.size()!=1 || !compat["webApiVersion"].is<uint32_t>() || compat["webApiVersion"].as<uint32_t>()!=ApiVersion) return fail(r,"api-version");
    JsonArrayConst files=root["files"].as<JsonArrayConst>();
    if (files.isNull() || files.size()!=count || !root["fileCount"].is<uint32_t>() || root["fileCount"].as<uint32_t>()!=count) return fail(r,"file-count");
    uint32_t seen=0, total=0;
    for (JsonVariantConst v : files) {
        JsonObjectConst f=v.as<JsonObjectConst>();
        if (f.isNull() || f.size()!=3) return fail(r,"file-structure");
        const char* path=f["path"].as<const char*>();
        size_t i=0;
        if (!path) return fail(r,"file-path");
        while (i<count && strcmp(path,assets[i])) ++i;
        if (i==count) return fail(r,"file-path");
        const uint32_t bit=uint32_t(1)<<i;
        if (seen&bit) return fail(r,"duplicate-file");
        seen|=bit;
        if (!f["size"].is<uint32_t>()) return fail(r,"file-size");
        const uint32_t size=f["size"].as<uint32_t>();
        if (!size || size>128UL*1024) return fail(r,"file-size");
        total+=size;
        if (total>384UL*1024) return fail(r,"total-size");
        if (!hashValid(f["sha256"].as<const char*>())) return fail(r,"sha256");
    }
    if (!root["totalBytes"].is<uint32_t>() || root["totalBytes"].as<uint32_t>()!=total) return fail(r,"total-size");
    r.fileCount=count; r.totalBytes=total;
    memcpy(r.version,version,strlen(version)+1);
    return true;
}
bool validate(const char* body, size_t length, Result& r) {
    return validateImpl(body, length, r);
}
bool validateMutable(char* body, size_t length, Result& r) {
    return validateImpl(body, length, r);
}

}
