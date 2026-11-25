#pragma once
#include <Arduino.h>

namespace WebManifest {
constexpr size_t MaxBytes = 4096;
constexpr uint16_t ApiVersion = 1;
constexpr size_t AssetCount = 12;
const char* assetName(size_t index);
int assetIndex(const char* name);
struct Result {
    const char* error;
    uint16_t fileCount;
    uint32_t totalBytes;
    char version[33];
    uint32_t sizes[AssetCount];
};
// Mutable variant consumes the input buffer (ArduinoJson zero-copy).
bool validateMutable(char* body, size_t length, Result& result);
bool validate(const char* body, size_t length, Result& result);
inline bool validate(const String& body, Result& result) {
    return validate(body.c_str(), body.length(), result);
}
}
