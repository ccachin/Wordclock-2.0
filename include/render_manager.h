#pragma once
#include "display_state.h"

namespace RenderManager
{

    const DisplayState& displayState();
    void begin();
    void update();

}