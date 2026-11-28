#pragma once
#include <stdint.h>

namespace RenderManager {
    // Values are part of the read-only /api/display wire contract.
    enum class DisplayMode : uint8_t { Theme, Weather, Night, Program, Preview, Waiting, Off };
    struct DisplayState {
        DisplayMode mode = DisplayMode::Waiting;
        uint8_t weather = 0; // 0 off, 1 applied, 2 waiting, 3 zero intensity, 4 paused
        uint8_t zone = 0;
        bool animated = false;
        bool ready = false;
        bool capped = false;
    };
    // Inputs come from the actual renderer, after priority resolution and cap.
    inline DisplayState describeFrame(bool powered, bool night, bool timeValid,
        bool preview, bool programTheme, bool weatherApplied, bool capped,
        bool weatherEnabled, bool weatherValid, bool weatherIntensity,
        uint8_t zone, bool maintenance, bool animated) {
        DisplayState d;
        d.mode = !powered ? DisplayMode::Off : night ? DisplayMode::Night :
            !timeValid ? DisplayMode::Waiting : preview ? DisplayMode::Preview :
            programTheme ? DisplayMode::Program : weatherApplied ? DisplayMode::Weather :
            capped ? DisplayMode::Program : DisplayMode::Theme;
        d.weather = !weatherEnabled ? 0 : d.mode==DisplayMode::Weather ? 1 :
            (night || preview || programTheme || !powered || !timeValid || maintenance) ? 4 :
            !weatherIntensity ? 3 : !weatherValid ? 2 : 4;
        d.zone=zone; d.capped=capped; d.animated=powered && animated; d.ready=true;
        return d;
    }
    // RGB values are globally scaled only for the preview. The real LED buffer is unchanged.
    inline uint8_t previewChannel(uint8_t channel,uint8_t level) {
        return (uint16_t(channel)*level+127)/255;
    }
    inline char hexNibble(uint8_t value) {
        return char(value<10?'0'+value:'a'+value-10);
    }
}
