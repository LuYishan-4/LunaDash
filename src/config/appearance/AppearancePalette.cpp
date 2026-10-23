#include "config/appearance/AppearancePalette.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace LunaDash {
namespace {
struct HslColor {
  double hue = 0.0;
  double saturation = 0.0;
  double lightness = 0.0;
};

int hexDigit(char value) {
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return -1;
}

std::array<int, 3> parseRgb(std::string_view value) {
  if (value.size() == 4 && value.front() == '#') {
    std::array<int, 3> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
      const int digit = hexDigit(value[index + 1]);
      if (digit < 0)
        return {156, 203, 251};
      result[index] = digit * 17;
    }
    return result;
  }
  if (value.size() != 7 || value.front() != '#')
    return {156, 203, 251};
  std::array<int, 3> result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    const int high = hexDigit(value[1 + index * 2]);
    const int low = hexDigit(value[2 + index * 2]);
    if (high < 0 || low < 0)
      return {156, 203, 251};
    result[index] = high * 16 + low;
  }
  return result;
}

HslColor toHsl(const std::array<int, 3> &rgb) {
  const double red = rgb[0] / 255.0;
  const double green = rgb[1] / 255.0;
  const double blue = rgb[2] / 255.0;
  const double maximum = std::max({red, green, blue});
  const double minimum = std::min({red, green, blue});
  const double delta = maximum - minimum;
  HslColor result;
  result.lightness = (maximum + minimum) / 2.0;
  if (delta == 0.0)
    return result;

  result.saturation = delta / (1.0 - std::abs(2.0 * result.lightness - 1.0));
  if (maximum == red)
    result.hue = std::fmod((green - blue) / delta, 6.0) / 6.0;
  else if (maximum == green)
    result.hue = ((blue - red) / delta + 2.0) / 6.0;
  else
    result.hue = ((red - green) / delta + 4.0) / 6.0;
  if (result.hue < 0.0)
    result.hue += 1.0;
  return result;
}

std::string fromHsl(double hue, double saturation, double lightness) {
  const double chroma =
      (1.0 - std::abs(2.0 * lightness - 1.0)) * saturation;
  const double sector = std::fmod(hue * 6.0, 6.0);
  const double secondary = chroma * (1.0 - std::abs(std::fmod(sector, 2.0) - 1.0));
  double red = 0.0;
  double green = 0.0;
  double blue = 0.0;
  if (sector < 1.0) {
    red = chroma;
    green = secondary;
  } else if (sector < 2.0) {
    red = secondary;
    green = chroma;
  } else if (sector < 3.0) {
    green = chroma;
    blue = secondary;
  } else if (sector < 4.0) {
    green = secondary;
    blue = chroma;
  } else if (sector < 5.0) {
    red = secondary;
    blue = chroma;
  } else {
    red = chroma;
    blue = secondary;
  }
  const double offset = lightness - chroma / 2.0;
  const auto channel = [offset](double value) {
    return static_cast<int>(std::lround(std::clamp(value + offset, 0.0, 1.0) *
                                        255.0));
  };
  std::ostringstream output;
  output << '#' << std::hex << std::setfill('0') << std::nouppercase
         << std::setw(2) << channel(red) << std::setw(2) << channel(green)
         << std::setw(2) << channel(blue);
  return output.str();
}

int localHour() {
  const auto now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  std::tm local{};
  return ::localtime_r(&now, &local) ? local.tm_hour : 12;
}
} // namespace

bool appearanceIsDark(const nlohmann::json &preferences) {
  const auto modeValue = preferences.value("themeMode", nlohmann::json("dark"));
  const auto mode = modeValue.is_string() ? modeValue.get<std::string>()
                                          : std::string("dark");
  const int hour = localHour();
  return mode == "dark" || (mode == "auto" && (hour < 7 || hour >= 19));
}

nlohmann::json appearancePalette(const nlohmann::json &preferences) {
  const bool dark = appearanceIsDark(preferences);
  const auto accentValue =
      preferences.value("accent", nlohmann::json("#9ccbfb"));
  const auto accent = accentValue.is_string()
                          ? accentValue.get<std::string>()
                          : std::string("#9ccbfb");
  const auto seed = toHsl(parseRgb(accent));
  const double hue = std::max(0.0, seed.hue);
  const double saturation = std::clamp(seed.saturation, 0.25, 0.65);
  const auto tone = [hue](double saturationValue, double lightness) {
    return fromHsl(hue, saturationValue, lightness);
  };
  const std::string accentTone = tone(saturation, dark ? 0.80 : 0.38);
  return {{"dark", dark},
          {"accent", accentTone},
          {"secondaryAccent", tone(0.22, dark ? 0.72 : 0.42)},
          {"background", tone(0.15, dark ? 0.065 : 0.975)},
          {"surface", tone(0.14, dark ? 0.10 : 0.95)},
          {"surfaceElevated", tone(0.15, dark ? 0.15 : 0.91)},
          {"surfaceHover", tone(0.18, dark ? 0.20 : 0.86)},
          {"text", tone(0.16, dark ? 0.92 : 0.13)},
          {"muted", tone(0.10, dark ? 0.71 : 0.40)},
          {"border", tone(0.12, dark ? 0.36 : 0.67)},
          {"hairline", tone(0.10, dark ? 0.23 : 0.83)},
          {"accentInk", tone(0.25, dark ? 0.15 : 0.99)},
          {"danger", dark ? "#ffb4ab" : "#ba1a1a"},
          {"success", dark ? "#b4d7ad" : "#386a32"},
          {"warning", dark ? "#efcd8c" : "#765900"}};
}
} // namespace LunaDash
