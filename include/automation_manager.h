#pragma once
#include "automation_rules.h"
#include "theme_manager.h"
namespace AutomationManager {
void begin();
void update(uint32_t now,bool eligible);
void apply(ThemeManager::Theme& theme,bool eligible);
uint32_t revision();
void encode(char*out);
bool replace(const char*hex,size_t length);
bool reset();
}
