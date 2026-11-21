#include "leds.h"
#include "config.h"

namespace Leds
{

    static CRGB pixels[Config::TOTAL_LED_COUNT];

    void begin()
    {
        FastLED.addLeds<WS2812, Config::LED_PIN, GRB>(
            pixels,
            Config::TOTAL_LED_COUNT);

        setBrightness(Config::DEFAULT_BRIGHTNESS);

        clear();
        show();
    }

    void show()
    {
        FastLED.show();
    }

    void clear()
    {
        fill_solid(
            pixels,
            Config::TOTAL_LED_COUNT,
            CRGB::Black);
    }

    void setBrightness(uint8_t brightness)
    {
        FastLED.setBrightness(
            min(brightness, Config::MAX_BRIGHTNESS));
    }

    CRGB getPixel(uint16_t index)
    {
        return index < Config::TOTAL_LED_COUNT ? pixels[index] : CRGB(0, 0, 0);
    }

    void setPixel(uint16_t index, CRGB color)
    {
        if (index < Config::TOTAL_LED_COUNT)
        {
            pixels[index] = color;
        }
    }

    void setFrontPixel(uint8_t x, uint8_t y, CRGB color)
    {
        if (x < Config::MATRIX_WIDTH &&
            y < Config::MATRIX_HEIGHT)
        {

            setPixel(Config::ledIndex(x, y), color);
        }
    }

    void setRearPixel(uint8_t index, CRGB color)
    {
        if (index < Config::REAR_LED_COUNT)
        {
            setPixel(Config::REAR_LED_START + index, color);
        }
    }

    void clearFront()
    {
        fillFront(CRGB::Black);
    }

    void clearRear()
    {
        fillRear(CRGB::Black);
    }

    void fillFront(CRGB color)
    {
        fill_solid(
            pixels,
            Config::FRONT_LED_COUNT,
            color);
    }

    void fillRear(CRGB color)
    {
        fill_solid(
            pixels + Config::REAR_LED_START,
            Config::REAR_LED_COUNT,
            color);
    }

}