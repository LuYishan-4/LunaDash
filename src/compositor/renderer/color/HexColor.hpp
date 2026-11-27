#pragma once

#include <cstdlib>
#include <string>

namespace LunaDash {

// "#rrggbb" into opaque RGBA; false when it is not one.
inline bool parseHexColor(const std::string &text, float rgba[4]) {
  if (text.size() != 7 || text.front() != '#')
    return false;
  for (int channel = 0; channel < 3; ++channel) {
    const auto value = std::strtol(text.substr(1 + channel * 2, 2).c_str(), nullptr, 16);
    rgba[channel] = static_cast<float>(value) / 255.0f;
  }
  rgba[3] = 1.0f;
  return true;
}

} // namespace LunaDash
