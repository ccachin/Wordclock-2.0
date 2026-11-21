#pragma once

#include <Arduino.h>

namespace Storage
{

    bool begin();
    bool isReady();

    size_t getTotalBytes();
    size_t getUsedBytes();
    size_t getFreeBytes();

}