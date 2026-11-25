#pragma once
#include <Arduino.h>

namespace WebSlotManager {
    enum class Slot : uint8_t { A, B };
    void begin();
    Slot activeSlot();
    Slot inactiveSlot();
    const char* root(Slot slot);
    const char* activeRoot();
    bool hasEntryPoint(Slot slot);
}
