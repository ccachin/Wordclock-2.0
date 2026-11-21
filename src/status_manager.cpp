#include "status_manager.h"
#include "leds.h"

namespace StatusManager
{

    constexpr uint8_t STATUS_LED = 2;

    constexpr uint32_t PULSE_PERIOD = 2400;
    constexpr uint32_t CONFIRM_DURATION = 2000;
    constexpr uint32_t UPDATE_INTERVAL = 20;

    static State currentState = State::Starting;

    static uint32_t stateStart = 0;
    static uint32_t lastUpdate = 0;

    void begin()
    {
        stateStart = millis();
        lastUpdate = millis();
    }

    void setState(State state)
    {

        if (state == currentState)
        {
            return;
        }

        currentState = state;
        stateStart = millis();

        Leds::setRearPixel(STATUS_LED, CRGB::Black);
        Leds::show();
    }

    State getState()
    {
        return currentState;
    }

    static CRGB getPulseColor(CRGB color, uint32_t elapsed)
    {

        uint8_t phase = static_cast<uint8_t>(
            ((elapsed % PULSE_PERIOD) * 256UL) / PULSE_PERIOD);

        uint8_t brightness = sin8(phase - 64);

        color.nscale8_video(brightness);

        return color;
    }

    void update()
    {

        if (currentState == State::Ready)
        {
            return;
        }

        uint32_t now = millis();

        if (now - lastUpdate < UPDATE_INTERVAL)
        {
            return;
        }

        lastUpdate = now;

        uint32_t elapsed = now - stateStart;

        CRGB color = CRGB::Black;

        switch (currentState)
        {

        case State::Starting:
            color = getPulseColor(
                CRGB::White, elapsed);
            break;

        case State::WifiConnecting:
            color = getPulseColor(
                CRGB::Blue, elapsed);
            break;

        case State::WifiSetupRequired:
            color = getPulseColor(
                CRGB::Orange, elapsed);
            break;

        case State::TimeSyncPending:
            color = getPulseColor(
                CRGB::Purple, elapsed);
            break;

        case State::Connected:

            if (elapsed >= CONFIRM_DURATION)
            {
                setState(State::Ready);
                return;
            }

            color = CRGB::Green;
            break;

        case State::Ready:
            return;
        }

        Leds::setRearPixel(STATUS_LED, color);
        Leds::show();
    }

}