#pragma once

#include <Arduino.h>

namespace ThemeLibrary
{

    String getCatalog();

    bool applyTheme(const String &id);

}