#include "config/appearance/AppearancePalette.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace LunaDash {
namespace {

void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

bool isHexColor(const std::string &value) {
  if (value.size() != 7 || value.front() != '#')
    return false;
  for (std::size_t index = 1; index < value.size(); ++index)
    if (!((value[index] >= '0' && value[index] <= '9') ||
          (value[index] >= 'a' && value[index] <= 'f')))
      return false;
  return true;
}

} // namespace
} // namespace LunaDash

int main() {
  try {
    const auto dark = LunaDash::appearancePalette(
        {{"themeMode", "dark"}, {"accent", "#6699cc"}});
    const auto light = LunaDash::appearancePalette(
        {{"themeMode", "light"}, {"accent", "#69c"}});
    const auto invalid = LunaDash::appearancePalette(
        {{"themeMode", "dark"}, {"accent", "invalid"}});
    const auto fallback = LunaDash::appearancePalette(
        {{"themeMode", "dark"}, {"accent", "#9ccbfb"}});

    LunaDash::require(dark.at("dark").get<bool>(),
                      "Dark mode was not respected.");
    LunaDash::require(!light.at("dark").get<bool>(),
                      "Light mode was not respected.");
    LunaDash::require(dark.at("accent") != light.at("accent"),
                      "Accent tone did not account for the color mode.");
    LunaDash::require(
        LunaDash::isHexColor(dark.at("background").get<std::string>()) &&
            LunaDash::isHexColor(light.at("text").get<std::string>()),
                      "Palette colors must be lowercase RGB hex values.");
    LunaDash::require(invalid == fallback,
                      "Invalid accent colors did not use the default seed.");
  } catch (const std::exception &error) {
    std::cerr << "Appearance palette test failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
