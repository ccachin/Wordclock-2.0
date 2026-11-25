#include <Arduino.h>
#include "firmware_update.h"
#include "leds.h"
#include "config.h"

namespace FirmwareUpdate
{
    static unsigned int lastProgress = 101;
    static Owner owner = Owner::None;

    // Preserve the rendered frame so an interrupted update can restore it
    // even when RenderManager does not need to draw a new frame.
    static CRGB savedFrame[Config::TOTAL_LED_COUNT];
    static uint8_t savedBrightness = 0;
    static bool frameSaved = false;
    static uint8_t displayedSteps = 255;

    static void showProgress(uint8_t steps)
    {
        if (steps == displayedSteps) return;
        displayedSteps = steps;
        Leds::clear();
        Leds::setBrightness(32);
        for (uint8_t i = 0; i < 4; ++i)
        {
            Leds::setPixel(Config::MINUTE_LEDS[i],
                           i < steps ? CRGB(0, 160, 255) : CRGB(0, 0, 35));
        }
        Leds::show();
    }

    static void restoreFrame()
    {
        if (!frameSaved) return;
        for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i)
            Leds::setPixel(i, savedFrame[i]);
        Leds::setBrightness(savedBrightness);
        Leds::show();
        frameSaved = false;
    }

    bool begin(Owner requested)
    {
        if (requested == Owner::None || owner != Owner::None) return false;
        owner = requested;
        lastProgress = 101;
        savedBrightness = FastLED.getBrightness();
        for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i)
            savedFrame[i] = Leds::getPixel(i);
        frameSaved = true;
        displayedSteps = 255;
        showProgress(0);
        return true;
    }

    bool active() { return owner != Owner::None; }

    void progress(Owner requested, unsigned int received, unsigned int total)
    {
        if (requested == Owner::None || owner != requested || !total) return;
        unsigned int percentage = received >= total ? 100U : (received * 100UL) / total;
        showProgress(static_cast<uint8_t>(min(percentage / 25U, 3U)));
        if (percentage != lastProgress && (percentage % 10 == 0 || percentage == 100))
        {
            lastProgress = percentage;
            Serial.printf("OTA progress: %u%%\n", percentage);
        }
    }

    void validated(Owner requested)
    {
        if (requested != Owner::None && owner == requested) showProgress(4);
        // Keep ownership until reboot.
    }

    void recover(Owner requested)
    {
        if (requested == Owner::None || owner != requested) return;
        restoreFrame();
        owner = Owner::None;
    }
}
