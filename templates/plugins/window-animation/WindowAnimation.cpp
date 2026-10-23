#include "core/plugins/PluginApi.h"
#include <nlohmann/json.hpp>

namespace LunaDash {
extern "C" int ludash_plugin_process(const char *request, char *response,
                                     size_t capacity) {
  using Json = nlohmann::json;
  const auto input = request ? Json::parse(request, nullptr, false) : Json{};
  if (!input.is_object())
    return ludash_plugin_write_json("{}", response, capacity);
  const bool replace =
      input.contains("mode") && input["mode"].is_string() &&
      input["mode"] == "replace";
  auto profile = input.value(replace ? "builtin" : "current", Json::object());
  const auto settings = input.value("settings", Json::object());
  if (!profile.is_object() || !settings.is_object())
    return ludash_plugin_write_json("{}", response, capacity);

  for (const auto &key : {"duration", "enterOffset", "focusOpacity",
                          "exitScale", "easing"})
    if (settings.contains(key))
      profile[key] = settings[key];

  const auto bytes = profile.dump();
  return ludash_plugin_write_json(bytes.c_str(), response, capacity);
}
} // namespace LunaDash
