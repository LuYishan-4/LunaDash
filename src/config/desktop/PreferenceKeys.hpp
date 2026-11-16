#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace LunaDash {

// A desktop preference name and the value used when it is unset. The store's
// defaults (DesktopPreferenceStore.cpp) are built from the same keys.
template <typename T> struct PreferenceKey {
  const char *name;
  T fallback;
};

namespace Preference {
inline constexpr PreferenceKey<bool> eyeCare{"eyeCare", false};
inline constexpr PreferenceKey<int> eyeCareTemperature{"eyeCareTemperature", 4500};
inline constexpr PreferenceKey<bool> blur{"blur", true};
inline constexpr PreferenceKey<int> blurRadius{"blurRadius", 18};
inline constexpr PreferenceKey<int> windowOpacity{"windowOpacity", 90};
inline constexpr PreferenceKey<bool> animations{"animations", true};
inline constexpr PreferenceKey<int> animationDuration{"animationDuration", 220};
inline constexpr PreferenceKey<int> gap{"gap", 12};
// Scrolling columns (niri-like) by default; "fill" splits the screen.
inline constexpr PreferenceKey<std::string_view> tilingMode{"tilingMode", "scroll"};
inline constexpr PreferenceKey<int> cursorSize{"cursorSize", 24};
inline constexpr PreferenceKey<std::string_view> keyboardLayout{"keyboardLayout", "us"};
inline constexpr PreferenceKey<std::string_view> updateChannel{"updateChannel", "stable"};
inline constexpr PreferenceKey<bool> weatherEnabled{"weatherEnabled", false};
inline constexpr PreferenceKey<double> weatherLatitude{"weatherLatitude", 0.0};
inline constexpr PreferenceKey<double> weatherLongitude{"weatherLongitude", 0.0};
inline constexpr PreferenceKey<std::string_view> weatherLocation{"weatherLocation", ""};
} // namespace Preference

template <typename T>
T preferenceValue(const nlohmann::json &preferences, const PreferenceKey<T> &key) {
  return preferences.value(key.name, key.fallback);
}

inline std::string preferenceValue(const nlohmann::json &preferences,
                                   const PreferenceKey<std::string_view> &key) {
  return preferences.value(key.name, std::string(key.fallback));
}

} // namespace LunaDash
