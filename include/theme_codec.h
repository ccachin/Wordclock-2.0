#pragma once
#include "theme_manager.h"
namespace ThemeCodec {
constexpr size_t MAX_BYTES = 2048;
struct Entry {
    String id, name, author, description;
    ThemeManager::Theme theme{};
};
bool validId(const String& id);
bool decode(const String& json, Entry& out, bool allowMissingId = false);
bool encode(const Entry& entry, String& json);
}
