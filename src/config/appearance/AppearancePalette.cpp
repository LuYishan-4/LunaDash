#include "config/appearance/AppearancePalette.hpp"
#include <QColor>
#include <QTime>
#include <algorithm>

namespace LunaDash {
bool appearanceIsDark(const QJsonObject &preferences) {
  const auto mode = preferences.value("themeMode").toString("dark");
  const int hour = QTime::currentTime().hour();
  return mode == "dark" || (mode == "auto" && (hour < 7 || hour >= 19));
}

QJsonObject appearancePalette(const QJsonObject &preferences) {
  const bool dark = appearanceIsDark(preferences);
  QColor seed(preferences.value("accent").toString("#9ccbfb"));
  if (!seed.isValid())
    seed = QColor("#9ccbfb");
  const double hue = std::max(0.0, double(seed.hslHueF()));
  const double saturation = std::clamp(seed.hslSaturationF(), 0.25f, 0.65f);
  const auto tone = [hue](double saturationValue, double lightness) {
    return QColor::fromHslF(hue, saturationValue, lightness).name();
  };
  const QString accent = tone(saturation, dark ? 0.80 : 0.38);
  return {{"dark", dark},
          {"accent", accent},
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
