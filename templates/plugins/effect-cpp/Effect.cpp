#include "core/plugins/PluginApi.h"
#include <QJsonDocument>
#include <QJsonObject>
namespace LunaDash {
extern "C" int ludash_plugin_process(const char *request, char *response, size_t capacity) {
    const auto input = QJsonDocument::fromJson(request).object();
    const auto settings = input.value("settings").toObject();
    auto rule = input.value("mode").toString() == "replace"
        ? input.value("builtin").toObject() : input.value("current").toObject();
    if (input.value("context").toObject().value("appId") == settings.value("appId"))
        rule["workspace"] = settings.value("workspace");
    const auto bytes = QJsonDocument(rule).toJson(QJsonDocument::Compact);
    return ludash_plugin_write_json(bytes.constData(), response, capacity);
}
} // namespace LunaDash
