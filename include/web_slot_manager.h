#pragma once
#include <Arduino.h>

namespace WebSlotManager {
    enum class Slot : uint8_t { A, B };
    void begin();
    Slot activeSlot();
    Slot inactiveSlot();
    const char* root(Slot slot);
    const char* activeRoot();
    bool isIncomplete(Slot slot);
    bool markIncomplete(Slot slot);
    bool hasEntryPoint(Slot slot);
    const char* selectionSource();
    uint32_t selectionGeneration();
    bool persistSelection(Slot slot);
}
