#pragma once

#include <Arduino.h>
#include "theme_manager.h"

namespace ThemeLibrary
{

    size_t factoryCount();
    const char* factoryEntry(size_t index);

    bool resolveFactory(size_t index, ThemeManager::Theme& theme);
    bool applyTheme(const String &id);

}