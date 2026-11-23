#include "storage.h"
#include <LittleFS.h>

namespace Storage
{

    static bool ready = false;

    bool begin()
    {

        LittleFSConfig config;
        config.setAutoFormat(false);
        LittleFS.setConfig(config);

        ready = LittleFS.begin();

        if (!ready)
        {
            Serial.println("LittleFS mount failed");
            return false;
        }

        Serial.println("LittleFS mounted");

        Serial.printf(
            "Total: %u bytes\n",
            getTotalBytes());

        Serial.printf(
            "Used: %u bytes\n",
            getUsedBytes());

        Serial.printf(
            "Free: %u bytes\n",
            getFreeBytes());

        return true;
    }

    bool isReady()
    {
        return ready;
    }

    static const char* RESET_MARKER = "/factory-reset.pending";

    bool factoryResetPending() { return ready && LittleFS.exists(RESET_MARKER); }

    bool requestFactoryReset() {
        if (!ready) return false;
        if (factoryResetPending()) return true;
        File f = LittleFS.open(RESET_MARKER, "w");
        if (!f) return false;
        f.close();
        return factoryResetPending();
    }

    bool clearFactoryUserData() {
        if (!factoryResetPending()) return false;
        const char* paths[] = {"/config/settings.json", "/config/settings.tmp", "/config/settings.bak"};
        for (const char* path : paths) {
            if (LittleFS.exists(path) && !LittleFS.remove(path)) return false;
        }
        // Reopen the iterator after each removal to avoid invalidation.
        const char* dir = "/themes/custom";
        if (LittleFS.exists(dir)) {
            while (true) {
                Dir d = LittleFS.openDir(dir);
                if (!d.next()) break;
                String name = d.fileName();
                if (d.isDirectory() || name.length() == 0 || name.indexOf('/') >= 0) return false;
                String path = String(dir) + "/" + name;
                if (!LittleFS.remove(path)) return false;
                yield();
            }
        }
        return true;
    }

    bool finishFactoryReset() {
        return factoryResetPending() && LittleFS.remove(RESET_MARKER) && !factoryResetPending();
    }

    size_t getTotalBytes()
    {

        FSInfo info;

        if (!ready || !LittleFS.info(info))
        {
            return 0;
        }

        return info.totalBytes;
    }

    size_t getUsedBytes()
    {

        FSInfo info;

        if (!ready || !LittleFS.info(info))
        {
            return 0;
        }

        return info.usedBytes;
    }

    size_t getFreeBytes()
    {
        return getTotalBytes() - getUsedBytes();
    }

}