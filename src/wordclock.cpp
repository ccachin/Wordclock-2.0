#include "wordclock.h"
#include "leds.h"
#include "config.h"

namespace Wordclock
{

    struct Word
    {
        uint16_t start;
        uint8_t length;
    };

    // Common words
    constexpr Word IL = {0, 2};
    constexpr Word EST = {3, 3};

    // Hours
    constexpr Word UNE = {33, 3};
    constexpr Word DEUX = {7, 4};
    constexpr Word TROIS = {49, 5};
    constexpr Word QUATRE = {11, 6};
    constexpr Word CINQ_H = {18, 4};
    constexpr Word SIX = {22, 3};
    constexpr Word SEPT = {37, 4};
    constexpr Word HUIT = {44, 4};
    constexpr Word NEUF = {25, 4};
    constexpr Word DIX_H = {41, 3};
    constexpr Word ONZE = {29, 4};
    constexpr Word MIDI = {56, 4};
    constexpr Word MINUIT = {66, 6};

    constexpr Word HEURE = {60, 5};
    constexpr Word HEURES = {60, 6};

    // Minutes
    constexpr Word ET = {73, 2};
    constexpr Word MOINS = {77, 5};
    constexpr Word VINGT = {83, 5};
    constexpr Word DIX_M = {88, 3};
    constexpr Word LE = {92, 2};
    constexpr Word DEMI = {95, 4};
    constexpr Word QUART = {99, 5};
    constexpr Word CINQ_M = {105, 4};

    // Display a word
    static void showWord(Word word, CRGB color)
    {

        for (uint8_t i = 0; i < word.length; i++)
        {
            Leds::setPixel(word.start + i, color);
        }
    }

    // Display the hour
    static void showHour(uint8_t hour, CRGB color)
    {

        if (hour == 0)
        {
            showWord(MINUIT, color);
            return;
        }

        if (hour == 12)
        {
            showWord(MIDI, color);
            return;
        }

        uint8_t h = hour % 12;

        switch (h)
        {
        case 1:
            showWord(UNE, color);
            break;
        case 2:
            showWord(DEUX, color);
            break;
        case 3:
            showWord(TROIS, color);
            break;
        case 4:
            showWord(QUATRE, color);
            break;
        case 5:
            showWord(CINQ_H, color);
            break;
        case 6:
            showWord(SIX, color);
            break;
        case 7:
            showWord(SEPT, color);
            break;
        case 8:
            showWord(HUIT, color);
            break;
        case 9:
            showWord(NEUF, color);
            break;
        case 10:
            showWord(DIX_H, color);
            break;
        case 11:
            showWord(ONZE, color);
            break;
        }

        if (h == 1)
        {
            showWord(HEURE, color);
        }
        else
        {
            showWord(HEURES, color);
        }
    }

    // Display minutes in five-minute steps
    static void showMinutes(uint8_t step, CRGB color)
    {

        switch (step)
        {

        case 0:
            break;

        case 5:
            showWord(CINQ_M, color);
            break;

        case 10:
            showWord(DIX_M, color);
            break;

        case 15:
            showWord(ET, color);
            showWord(QUART, color);
            break;

        case 20:
            showWord(VINGT, color);
            break;

        case 25:
            showWord(VINGT, color);
            showWord(CINQ_M, color);
            break;

        case 30:
            showWord(ET, color);
            showWord(DEMI, color);
            break;

        case 35:
            showWord(MOINS, color);
            showWord(VINGT, color);
            showWord(CINQ_M, color);
            break;

        case 40:
            showWord(MOINS, color);
            showWord(VINGT, color);
            break;

        case 45:
            showWord(MOINS, color);
            showWord(LE, color);
            showWord(QUART, color);
            break;

        case 50:
            showWord(MOINS, color);
            showWord(DIX_M, color);
            break;

        case 55:
            showWord(MOINS, color);
            showWord(CINQ_M, color);
            break;
        }
    }

    // Display remaining minutes using stars
    static void showMinuteIndicators(
        uint8_t remaining,
        CRGB color)
    {

        for (uint8_t i = 0; i < remaining; i++)
        {
            Leds::setPixel(
                Config::MINUTE_LEDS[i],
                color);
        }
    }

    // Main display function
    void displayTime(
        uint8_t hour,
        uint8_t minute,
        CRGB color)
    {

        if (hour > 23 || minute > 59)
        {
            return;
        }

        Leds::clearFront();

        uint8_t step = (minute / 5) * 5;
        uint8_t remaining = minute % 5;

        if (step >= 35)
        {
            hour = (hour + 1) % 24;
        }

        showWord(IL, color);
        showWord(EST, color);

        showHour(hour, color);
        showMinutes(step, color);
        showMinuteIndicators(remaining, color);
    }

}