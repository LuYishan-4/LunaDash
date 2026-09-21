#include "core/plugins/PluginApi.h"
#include <QJsonDocument>
#include <QJsonObject>

namespace LunaDash {
extern "C" int ludash_plugin_process(const char *request, char *response,
                                     size_t capacity) {
  const auto input = QJsonDocument::fromJson(request).object();
  auto profile = input.value(input.value("mode").toString() == "replace"
                                 ? "builtin"
                                 : "current")
                     .toObject();
  const auto settings = input.value("settings").toObject();

  for (const auto &key : {"duration", "enterOffset", "focusOpacity",
                          "exitScale", "easing"})
    if (settings.contains(key))
      profile[key] = settings.value(key);

  const auto bytes = QJsonDocument(profile).toJson(QJsonDocument::Compact);
  return ludash_plugin_write_json(bytes.constData(), response, capacity);
}
} // namespace LunaDash
