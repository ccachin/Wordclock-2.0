#pragma once
// Single source for the public JSON catalogue and the typed firmware presets.
#define WC_FACTORY_THEME_ROWS(X) \
    X(warm_white, "warm-white", "Warm White", FFE1B4, B87838, Solid, "solid", 50) \
    X(ocean, "ocean", "Ocean", A9EBFF, 267DD9, Solid, "solid", 50) \
    X(sunset, "sunset", "Sunset", FFB568, A63372, Solid, "solid", 50) \
    X(forest, "forest", "Forest", C9E8A5, 296B48, Solid, "solid", 50) \
    X(aurora, "aurora", "Aurora", 79F2D0, 7848D8, Solid, "solid", 50) \
    X(ocean_pulse, "ocean-pulse", "Ocean Pulse", A9EBFF, 267DD9, Breathing, "breathing", 70) \
    X(aurora_flow, "aurora-flow", "Aurora Flow", 79F2D0, 7848D8, ColorCycle, "color-cycle", 50) \
    X(rainbow, "rainbow", "Rainbow", FFE1B4, FFFFFF, Rainbow, "rainbow", 65) \
    X(ember, "ember", "Ember", FFB568, C53D10, Chase, "chase", 85)

#define WC_FACTORY_THEME_JSON(id, name, front, rear, effect, speed) \
    "{\"version\":2,\"id\":\"" id "\",\"name\":\"" name "\",\"author\":\"Wordclock\",\"front\":\"#" #front "\",\"rear\":\"#" #rear "\",\"frontEffect\":\"solid\",\"rearEffect\":\"" effect "\",\"frontSpeed\":50,\"rearSpeed\":" #speed "}"
#define WC_FACTORY_THEME_RGB(hex) 0x##hex##UL
