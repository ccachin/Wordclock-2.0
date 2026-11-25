#pragma once
#include <Arduino.h>

namespace WebManifest {
constexpr size_t MaxBytes = 8192;
constexpr uint16_t ApiVersion = 1;
struct Result {
    const char* error;
    uint16_t fileCount;
    uint32_t totalBytes;
    char version[33];
};
bool validate(const char* body, size_t length, Result& result);
inline bool validate(const String& body, Result& result) {
    return validate(body.c_str(), body.length(), result);
}
}
