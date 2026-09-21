#include "core/plugins/PluginApi.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace LunaDash {

extern "C" int ludash_plugin_process(const char *request, char *response,
                                     size_t capacity) {
  const auto input = QJsonDocument::fromJson(request).object();
  auto profile = input.value("mode").toString() == "augment"
                     ? input.value("current").toObject()
                     : input.value("builtin").toObject();

  profile["duration"] =
      input.value("settings").toObject().value("duration").toInt(260);
  profile["enterOffset"] = 0;
  profile["exitScale"] = 1.0;
  profile["easing"] = "outQuint";

  const auto json = QJsonDocument(profile).toJson(QJsonDocument::Compact);
  return ludash_plugin_write_json(json.constData(), response, capacity);
}

} // namespace LunaDash
