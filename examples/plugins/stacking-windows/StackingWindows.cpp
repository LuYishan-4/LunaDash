#include "core/plugins/PluginApi.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace LunaDash {
extern "C" int ludash_plugin_process(const char *request, char *response,
                                     size_t capacity) {
  const auto input = QJsonDocument::fromJson(request).object();
  auto result = input.value("builtin").toObject();
  auto windows = result.value("windows").toArray();
  const auto context = input.value("context").toObject();
  const auto fresh = context.value("newWindows").toArray();
  const auto area = context.value("area").toObject();
  const int step =
      input.value("settings").toObject().value("cascadeStep").toInt(32);
  for (int index = 0; index < windows.size(); ++index) {
    auto window = windows[index].toObject();
    if (!fresh.contains(window.value("id")))
      continue; // Preserve manual positions and sizes across every later hook.
    const int offset = step * (index % 7);
    window["x"] = area.value("x").toInt() +
                  std::min(offset, area.value("width").toInt() -
                                       window.value("width").toInt());
    window["y"] = area.value("y").toInt() +
                  std::min(offset, area.value("height").toInt() -
                                       window.value("height").toInt());
    windows[index] = window;
  }
  result["windows"] = windows;
  const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Compact);
  return ludash_plugin_write_json(bytes.constData(), response, capacity);
}
} // namespace LunaDash
