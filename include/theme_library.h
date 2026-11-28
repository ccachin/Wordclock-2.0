#pragma once

#include <Arduino.h>
#include "theme_manager.h"

namespace ThemeLibrary
{

    size_t factoryCount();
    constexpr size_t FactoryEntryCapacity = 256;
    bool factoryEntry(size_t index, char* output, size_t capacity);

    bool resolveFactory(size_t index, ThemeManager::Theme& theme);
    bool applyTheme(const String &id);

}