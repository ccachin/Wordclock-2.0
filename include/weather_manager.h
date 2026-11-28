#pragma once
#include "theme_manager.h"
namespace WeatherManager {
struct Config { int32_t lat=0, lon=0; uint8_t enabled=0, forecast=0, zone=0, intensity=30; };
void begin(); bool save(const Config&); bool reset(); const Config& config();
void update(bool allowed); void refresh(); uint32_t revision();
void apply(ThemeManager::Theme&, bool allowed); bool valid(); int code(); int day(); int temperature(); uint32_t age(); const char* status();
}
