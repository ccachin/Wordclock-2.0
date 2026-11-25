#include "web_slot_manager.h"
#include "storage.h"
#include <LittleFS.h>

namespace WebSlotManager {
    // Legacy installations always start on A. Persistent selection follows later.
    Slot activeSlot() { return Slot::A; }
    Slot inactiveSlot() { return activeSlot() == Slot::A ? Slot::B : Slot::A; }
    const char* root(Slot slot) {
        switch (slot) {
            case Slot::A: return "/www";
            case Slot::B: return "/www-b";
        }
        return nullptr;
    }
    const char* activeRoot() { return root(activeSlot()); }
    bool hasEntryPoint(Slot slot) {
        const char* directory = root(slot);
        if (!directory || !Storage::isReady()) return false;
        File file = LittleFS.open(String(directory) + "/index.html", "r");
        if (!file) return false;
        const bool valid = !file.isDirectory() && file.size() > 0;
        file.close();
        return valid;
    }
    void begin() {
        Serial.printf("Web slots: active=A (%s), inactive=B (%s)\n", root(activeSlot()), root(inactiveSlot()));
        if (!Storage::isReady()) {
            Serial.println("Web slots: storage unavailable");
            return;
        }
        Serial.printf("Web slots: entry point A=%s, B=%s\n",
                      hasEntryPoint(Slot::A) ? "present" : "missing",
                      hasEntryPoint(Slot::B) ? "present" : "missing");
    }
}
