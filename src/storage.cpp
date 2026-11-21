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