#pragma once

#include <Arduino.h>

namespace Config
{

    // Hardware
    constexpr uint8_t LED_PIN = 4;

    // Front matrix
    constexpr uint8_t MATRIX_WIDTH = 11;
    constexpr uint8_t MATRIX_HEIGHT = 11;

    constexpr uint16_t FRONT_LED_COUNT =
        MATRIX_WIDTH * MATRIX_HEIGHT;

    // Rear lighting
    constexpr uint16_t REAR_LED_COUNT = 20;
    constexpr uint16_t REAR_LED_START = FRONT_LED_COUNT;

    // Total LED count
    constexpr uint16_t TOTAL_LED_COUNT =
        FRONT_LED_COUNT + REAR_LED_COUNT;

    // Minute indicators
    constexpr uint16_t MINUTE_LEDS[] = {
        117, 118, 119, 120};

    // Brightness (0 - 255)
    constexpr uint8_t DEFAULT_BRIGHTNESS = 8;
    constexpr uint8_t MAX_BRIGHTNESS = 255;

    // Convert matrix coordinates to LED index
    constexpr uint16_t ledIndex(uint8_t x, uint8_t y)
    {
        return y * MATRIX_WIDTH + x;
    }

}