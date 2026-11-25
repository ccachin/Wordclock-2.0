#pragma once

#include <Arduino.h>

namespace ThemeLibrary
{

    size_t factoryCount();
    const char* factoryEntry(size_t index);

    bool applyTheme(const String &id);

}