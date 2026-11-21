#pragma once
#include "theme_codec.h"
namespace CustomThemeStore {
constexpr size_t MAX_THEMES=30;
bool load(const String& id,ThemeCodec::Entry& out);
bool create(ThemeCodec::Entry& entry);
bool update(const ThemeCodec::Entry& entry);
bool remove(const String& id);
size_t count();
bool listIds(String& json);
void runDiagnostics();
}
