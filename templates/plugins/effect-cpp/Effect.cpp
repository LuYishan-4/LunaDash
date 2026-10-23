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
  auto rule = input.value(replace ? "builtin" : "current", Json::object());
  auto settings = input.value("settings", Json::object());
  if (!rule.is_object())
    rule = Json::object();
  if (!settings.is_object())
    settings = Json::object();
  if (settings.contains("workspace"))
    rule["workspace"] = settings["workspace"];
  else
    rule.erase("workspace");
  if (settings.contains("maximize"))
    rule["maximized"] = settings["maximize"];
  else
    rule.erase("maximized");
  const auto bytes = rule.dump();
  return ludash_plugin_write_json(bytes.c_str(), response, capacity);
}
} // namespace LunaDash
