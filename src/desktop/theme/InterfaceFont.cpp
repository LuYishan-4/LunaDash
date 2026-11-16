#include "desktop/theme/InterfaceFont.hpp"

#include "config/desktop/DesktopPreferenceStore.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <sstream>
#include <vector>

namespace LunaDash {
namespace {

constexpr const char *interfaceSchema = "org.gnome.desktop.interface";
// Portal calls must not hold up a window that is about to open.
constexpr int portalTimeoutMs = 500;

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

bool isSize(std::string_view word) {
  if (word.ends_with("px"))
    word.remove_suffix(2);
  double value = 0.0;
  const auto result = std::from_chars(word.data(), word.data() + word.size(), value);
  return !word.empty() && result.ec == std::errc() &&
         result.ptr == word.data() + word.size();
}

// The setting through the portal, which works under any desktop that has one.
std::string portalSetting(const char *key) {
  GError *error = nullptr;
  GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
  if (!bus) {
    g_clear_error(&error);
    return {};
  }
  GVariant *reply = g_dbus_connection_call_sync(
      bus, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.Settings", "ReadOne",
      g_variant_new("(ss)", interfaceSchema, key), G_VARIANT_TYPE("(v)"),
      G_DBUS_CALL_FLAGS_NONE, portalTimeoutMs, nullptr, &error);
  g_object_unref(bus);
  if (!reply) {
    g_clear_error(&error);
    return {};
  }
  GVariant *boxed = nullptr;
  g_variant_get(reply, "(v)", &boxed);
  std::string value;
  if (boxed && g_variant_is_of_type(boxed, G_VARIANT_TYPE_STRING))
    value = g_variant_get_string(boxed, nullptr);
  if (boxed)
    g_variant_unref(boxed);
  g_variant_unref(reply);
  return value;
}

// GSettings directly, where the schema is installed but no portal runs.
std::string gsettingsSetting(const char *key) {
  auto *source = g_settings_schema_source_get_default();
  auto *schema =
      source ? g_settings_schema_source_lookup(source, interfaceSchema, TRUE) : nullptr;
  if (!schema)
    return {};
  const bool hasKey = g_settings_schema_has_key(schema, key);
  g_settings_schema_unref(schema);
  if (!hasKey)
    return {};
  GSettings *settings = g_settings_new(interfaceSchema);
  gchar *text = g_settings_get_string(settings, key);
  std::string value = text ? text : "";
  g_free(text);
  g_object_unref(settings);
  return value;
}

} // namespace

std::string fontDescriptionFamily(std::string_view description) {
  static constexpr std::array<std::string_view, 31> styles{
      "normal", "roman", "oblique", "italic", "small-caps", "thin", "ultra-light",
      "extra-light", "light", "semi-light", "demi-light", "book", "regular", "medium",
      "semi-bold", "demi-bold", "bold", "ultra-bold", "extra-bold", "heavy", "black",
      "ultra-heavy", "extra-heavy", "ultra-condensed", "extra-condensed", "condensed",
      "semi-condensed", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded"};
  std::vector<std::string> words;
  std::istringstream stream{std::string(description)};
  for (std::string word; stream >> word;)
    words.push_back(word);
  if (!words.empty() && isSize(words.back()))
    words.pop_back();
  while (!words.empty()) {
    auto word = lower(words.back());
    if (!word.empty() && word.back() == ',')
      word.pop_back();
    if (std::find(styles.begin(), styles.end(), word) == styles.end())
      break;
    words.pop_back();
  }
  std::string families;
  for (const auto &word : words)
    families += (families.empty() ? "" : " ") + word;
  auto family = families.substr(0, families.find(','));
  while (!family.empty() && std::isspace(static_cast<unsigned char>(family.back())))
    family.pop_back();
  return family;
}

std::string desktopFontFamily() {
  const auto preferences = loadDesktopPreferences();
  const auto found = preferences.find("fontFamily");
  const std::string generic = found != preferences.end() && found->is_string()
                                  ? found->get<std::string>()
                                  : std::string("sans-serif");
  const char *key = generic == "sans-serif"  ? "font-name"
                    : generic == "monospace" ? "monospace-font-name"
                                             : nullptr;
  if (!key)
    return generic;
  auto description = portalSetting(key);
  if (description.empty())
    description = gsettingsSetting(key);
  const auto family = fontDescriptionFamily(description);
  return family.empty() ? generic : family;
}

} // namespace LunaDash
