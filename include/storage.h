#pragma once

#include <Arduino.h>

namespace Storage
{

    bool begin();
    bool isReady();
    bool factoryResetPending();
    bool requestFactoryReset();
    bool clearFactoryUserData();
    bool finishFactoryReset();

    size_t getTotalBytes();
    size_t getUsedBytes();
    size_t getFreeBytes();

}