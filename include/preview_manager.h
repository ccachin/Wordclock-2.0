#pragma once
#include "theme_manager.h"
namespace PreviewManager {
    constexpr uint32_t TIMEOUT_MS = 10000;
    bool active();
    bool set(const String& token, const ThemeManager::Theme& theme);
    bool stop(const String& token);
    uint32_t revision();
    ThemeManager::Theme renderTheme();
}
