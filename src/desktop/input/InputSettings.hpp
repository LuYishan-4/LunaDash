#pragma once

#include <string>

namespace LunaDash {

std::string keyboardLayoutPreference(const std::string &requested = "us");
int keyboardRepeatRate();
int keyboardRepeatDelay();

} // namespace LunaDash
