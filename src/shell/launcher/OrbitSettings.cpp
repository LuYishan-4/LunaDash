#include "shell/launcher/OrbitSettings.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <limits>

int qInitResources_orbit_defaults();
namespace LunaDash {
namespace {
QString path() {
  return QStandardPaths::writableLocation(
             QStandardPaths::GenericConfigLocation) +
         "/lunadash/orbit.json";
}
QJsonObject defaults() {
  static const QJsonObject value = [] {
    ::qInitResources_orbit_defaults();
    QFile file(":/LunaDash/launcher/orbit.json");
    if (!file.open(QIODevice::ReadOnly))
      qFatal("Missing embedded Orbit configuration");
    return QJsonDocument::fromJson(file.readAll()).object();
  }();
  return value;
}

quint64 orbitRevision = 0;
bool shortText(const QJsonValue &value, int maximum = 128) {
  return value.isString() && !value.toString().isEmpty() &&
         value.toString().size() <= maximum &&
         !value.toString().contains(QChar::Null) &&
         !value.toString().contains('\n');
}
bool webUrl(const QJsonValue &value) {
  const QUrl url(value.toString());
  return shortText(value, 2048) && url.isValid() && !url.host().isEmpty() &&
         QStringList{"https", "http"}.contains(url.scheme());
}
bool itemsValid(const QJsonArray &items, int depth) {
  if (items.isEmpty() || items.size() > 8 || depth > 2)
    return false;
  QSet<QString> ids;
  for (const auto &entry : items) {
    if (!entry.isObject())
      return false;
    const auto item = entry.toObject();
    if (!shortText(item.value("id")) || !shortText(item.value("name")) ||
        ids.contains(item.value("id").toString()))
      return false;
    ids.insert(item.value("id").toString());
    int actions = 0;
    for (auto it = item.begin(); it != item.end(); ++it) {
      const auto key = it.key();
      if (key == "id" || key == "name")
        continue;
      if (key == "description" || key == "icon") {
        if (!shortText(it.value(), 256))
          return false;
        continue;
      }
      ++actions;
      if (key == "children") {
        if (!it.value().isArray() ||
            !itemsValid(it.value().toArray(), depth + 1))
          return false;
      } else if (key == "url") {
        if (!webUrl(it.value()))
          return false;
      } else if (key == "desktopId") {
        if (!shortText(it.value(), 256))
          return false;
      } else if (key == "action") {
        if (!QStringList{"terminal", "files", "browser", "settings",
                         "wallpapers", "eye-care", "scratchpad", "power",
                         "clipboard", "dashboard"}
                 .contains(it.value().toString()))
          return false;
      } else if (key == "command") {
        if (!it.value().isArray() || it.value().toArray().isEmpty() ||
            it.value().toArray().size() > 24)
          return false;
        for (const auto &argument : it.value().toArray())
          if (!shortText(argument, 1024))
            return false;
      } else {
        return false;
      }
    }
    if (actions != 1)
      return false;
  }
  return true;
}
bool validate(const QByteArray &text, QJsonObject *object, QString *error) {
  const auto document = QJsonDocument::fromJson(text);
  const auto root = document.object();
  bool ok = text.size() <= 32768 && document.isObject() && root.size() == 4 &&
            root.value("schemaVersion") == QJsonValue(1) &&
            root.value("items").isArray() &&
            itemsValid(root.value("items").toArray(), 0) &&
            root.value("searchEngines").isArray();
  const auto engines = root.value("searchEngines").toArray();
  QSet<QString> ids;
  ok = ok && !engines.isEmpty() && engines.size() <= 8;
  for (const auto &value : engines) {
    const auto engine = value.toObject();
    const auto id = engine.value("id").toString();
    ok = ok && engine.size() == 3 && shortText(engine.value("id")) &&
         shortText(engine.value("name")) && webUrl(engine.value("url")) &&
         engine.value("url").toString().count("{query}") == 1 &&
         !ids.contains(id);
    ids.insert(id);
  }
  ok = ok && ids.contains(root.value("defaultEngine").toString());
  if (!ok) {
    if (error)
      *error =
          "Orbit requires schemaVersion 1, a defaultEngine, 1–8 searchEngines "
          "and 1–8 items per folder (at most three levels). Each item needs "
          "one action, desktopId, command array, web URL or children.";
    return false;
  }
  *object = root;
  return true;
}
} // namespace
QJsonObject orbitSettings() {
  static QJsonObject cache;
  static qint64 cachedModified = -2;
  static qint64 cachedSize = -2;
  static quint64 cachedRevision = std::numeric_limits<quint64>::max();

  const QString settingsPath = path();
  const QFileInfo info(settingsPath);
  const qint64 modified =
      info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
  const qint64 size = info.exists() ? info.size() : -1;
  if (cachedRevision == orbitRevision && cachedModified == modified &&
      cachedSize == size)
    return cache;

  QJsonObject document = defaults();
  QString error;
  QFile file(settingsPath);
  if (file.exists()) {
    if (!file.open(QIODevice::ReadOnly))
      error = file.errorString();
    else
      validate(file.read(32769), &document, &error);
  }
  cache = {{"document", document}, {"path", settingsPath}, {"error", error}};
  cachedModified = modified;
  cachedSize = size;
  cachedRevision = orbitRevision;
  return cache;
}
bool saveOrbitSettings(const QByteArray &text, QString *error) {
  QJsonObject document;
  if (!validate(text, &document, error))
    return false;
  QDir().mkpath(QFileInfo(path()).absolutePath());
  QSaveFile file(path());
  const auto bytes = QJsonDocument(document).toJson();
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
      !file.commit()) {
    if (error)
      *error = file.errorString();
    return false;
  }
  ++orbitRevision;
  return true;
}
bool resetOrbitSettings(QString *error) {
  return saveOrbitSettings(QJsonDocument(defaults()).toJson(), error);
}
} // namespace LunaDash
