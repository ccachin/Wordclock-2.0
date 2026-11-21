
#include "status_manager.h"

namespace StatusManager
{

    constexpr uint32_t PULSE_PERIOD = 2400;
    constexpr uint32_t CONFIRM_DURATION = 2000;

    static State currentState = State::Starting;

    static uint32_t stateStartedAt = 0;

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {

        currentState = State::Starting;

        stateStartedAt = millis();
    }

    // ==================================================
    // State management
    // ==================================================

    void setState(State state)
    {

        if (currentState == state)
        {

            return;
        }

        currentState = state;

        stateStartedAt = millis();
    }

    State getState()
    {

        return currentState;
    }

    bool isActive()
    {

        return currentState != State::Ready;
    }

    // ==================================================
    // State update
    // ==================================================

    void update()
    {

        if (currentState == State::Connected)
        {

            if (millis() - stateStartedAt >=
                CONFIRM_DURATION)
            {

                setState(State::Ready);
            }
        }
    }

    // ==================================================
    // Pulse animation
    // ==================================================

    static CRGB getPulseColor(CRGB color)
    {

        uint32_t elapsed =
            millis() - stateStartedAt;

        uint8_t phase =
            static_cast<uint8_t>(
                (elapsed % PULSE_PERIOD) *
                256UL / PULSE_PERIOD);

        uint8_t intensity =
            sin8(static_cast<uint8_t>(phase - 64));

        color.nscale8_video(intensity);

        return color;
    }

    // ==================================================
    // Status color
    // ==================================================

    CRGB getColor()
    {

        switch (currentState)
        {

        case State::Starting:

            return getPulseColor(CRGB::White);

        case State::WifiConnecting:

            return getPulseColor(CRGB::Blue);

        case State::WifiSetupRequired:

            return getPulseColor(CRGB::Orange);

        case State::TimeSyncPending:

            return getPulseColor(CRGB::Purple);

        case State::Connected:

            return CRGB::Green;

        case State::Ready:

        default:

            return CRGB::Black;
        }
    }

}
