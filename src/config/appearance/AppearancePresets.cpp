#include "config/appearance/AppearancePresets.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace LunaDash {
namespace {
QString directory() {
  return QStandardPaths::writableLocation(
             QStandardPaths::GenericConfigLocation) +
         "/lunadash/presets";
}
bool validName(const QString &name, QString *error) {
  if (QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,47}$")
          .match(name)
          .hasMatch())
    return true;
  if (error)
    *error = "Preset names use 1–48 letters, digits, underscores or hyphens.";
  return false;
}
QStringList keys() {
  return {"accent",
          "secondaryAccent",
          "themeMode",
          "wallpaperColors",
          "gap",
          "panelHeight",
          "blur",
          "blurRadius",
          "windowOpacity",
          "animations",
          "animationDuration",
          "fontFamily",
          "eyeCare",
          "eyeCareTemperature",
          "dockEnabled",
          "dockAutoHide"};
}
} // namespace
QJsonArray appearancePresets() {
  QJsonArray result;
  const auto entries =
      QDir(directory())
          .entryInfoList({"*.json"}, QDir::Files | QDir::NoSymLinks,
                         QDir::Name);
  for (const auto &file : entries) {
    if (result.size() >= 64)
      break;
    if (validName(file.completeBaseName(), nullptr))
      result.append(QJsonObject{{"name", file.completeBaseName()}});
  }
  return result;
}
bool saveAppearancePreset(const QString &name, QString *error) {
  if (!validName(name, error))
    return false;
  if (appearancePresets().size() >= 64 &&
      !QFileInfo::exists(directory() + "/" + name + ".json")) {
    if (error)
      *error = "Keep at most 64 appearance presets.";
    return false;
  }
  QJsonObject values;
  const auto preferences = desktopPreferences();
  for (const auto &key : keys())
    values[key] = preferences.value(key);
  QDir().mkpath(directory());
  QSaveFile file(directory() + "/" + name + ".json");
  const auto bytes =
      QJsonDocument(QJsonObject{{"schemaVersion", 1}, {"appearance", values}})
          .toJson();
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
      !file.commit()) {
    if (error)
      *error = file.errorString();
    return false;
  }
  return true;
}
bool applyAppearancePreset(const QString &name, QString *error) {
  if (!validName(name, error))
    return false;
  QFile file(directory() + "/" + name + ".json");
  if (!file.open(QIODevice::ReadOnly)) {
    if (error)
      *error = file.errorString();
    return false;
  }
  const auto document = QJsonDocument::fromJson(file.read(16385));
  const auto root = document.object();
  const auto values = root.value("appearance").toObject();
  if (file.size() > 16384 || root.value("schemaVersion") != QJsonValue(1) ||
      values.isEmpty()) {
    if (error)
      *error = "Invalid appearance preset.";
    return false;
  }
  for (auto it = values.begin(); it != values.end(); ++it) {
    if (!keys().contains(it.key())) {
      if (error)
        *error = "Unknown preset setting: " + it.key();
      return false;
    }
  }
  return updateDesktopPreferences(values, error);
}
bool deleteAppearancePreset(const QString &name, QString *error) {
  if (!validName(name, error))
    return false;
  QFile file(directory() + "/" + name + ".json");
  if (file.remove())
    return true;
  if (error)
    *error = file.errorString();
  return false;
}
} // namespace LunaDash
