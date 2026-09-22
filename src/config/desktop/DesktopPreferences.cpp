#include "config/desktop/DesktopPreferences.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <cmath>

namespace LunaDash {
namespace {
QJsonObject defaults() {
  return {{"accent", "#9ccbfb"},
          {"secondaryAccent", "#41576b"},
          {"colorPins",
           QJsonArray{"#9ccbfb", "#c4b5fd", "#7dcccf", "#e7b899", "#41576b"}},
          {"gap", 12},
          {"panelHeight", 40},
          {"blur", true},
          {"blurRadius", 18},
          {"windowOpacity", 96},
          {"animations", true},
          {"animationDuration", 220},
          {"workspaceCount", 10},
          {"keyboardLayout", "us"},
          {"cursorSize", 24},
          {"fontFamily", "sans-serif"},
          {"clock24Hour", true},
          {"startupApps", QJsonArray{}},
          {"proxyEnabled", false},
          {"proxyHttp", ""},
          {"proxyHttps", ""},
          {"proxySocks", ""},
          {"proxyBypass", ""},
          {"overview", false},
          {"showHostDetails", false},
          {"updateChannel", "stable"}};
}

bool validProxyText(const QJsonValue &value, bool url) {
  if (!value.isString())
    return false;
  const QString text = value.toString();
  if (text.size() > 512 || text.contains('\n') || text.contains('\r') ||
      text.contains(QChar::Null))
    return false;
  if (!url || text.isEmpty())
    return true;
  const QUrl parsed(text);
  return parsed.isValid() &&
         QStringList{"http", "https", "socks", "socks5"}.contains(
             parsed.scheme().toLower());
}

bool validColor(const QJsonValue &value) {
  return value.isString() && QRegularExpression("^#[0-9a-fA-F]{6}$")
                                 .match(value.toString())
                                 .hasMatch();
}

bool valid(const QString &key, const QJsonValue &value) {
  if (key == "keyboardLayout")
    return value.isString() &&
           QStringList{"us", "gb", "de", "fr", "es", "jp", "tw"}.contains(
               value.toString());
  if (key == "fontFamily")
    return value.isString() &&
           QStringList{"sans-serif", "serif", "monospace"}.contains(
               value.toString());
  if (key == "updateChannel")
    return value.isString() &&
           QStringList{"stable", "dev"}.contains(value.toString());
  if (key == "proxyHttp" || key == "proxyHttps" || key == "proxySocks")
    return validProxyText(value, true);
  if (key == "proxyBypass")
    return validProxyText(value, false);
  if (key == "startupApps") {
    if (!value.isArray() || value.toArray().size() > 3)
      return false;
    QSet<QString> seen;
    for (const auto &app : value.toArray()) {
      if (!app.isString() ||
          !QStringList{"files", "packages", "welcome"}.contains(
              app.toString()) ||
          seen.contains(app.toString()))
        return false;
      seen.insert(app.toString());
    }
    return true;
  }
  if (key == "colorPins") {
    if (!value.isArray() || value.toArray().size() > 16)
      return false;
    QSet<QString> seen;
    for (const auto &color : value.toArray()) {
      if (!validColor(color))
        return false;
      const QString normalized = color.toString().toLower();
      if (seen.contains(normalized))
        return false;
      seen.insert(normalized);
    }
    return true;
  }
  const QMap<QString, QPair<int, int>> ranges{{"workspaceCount", {1, 10}},
                                              {"cursorSize", {16, 64}}};
  if (ranges.contains(key)) {
    const double number = value.toDouble(-1);
    const auto range = ranges.value(key);
    return value.isDouble() && std::isfinite(number) &&
           std::floor(number) == number && number >= range.first &&
           number <= range.second;
  }
  if (key == "accent" || key == "secondaryAccent")
    return validColor(value);
  if (key == "overview" || key == "showHostDetails" || key == "blur" ||
      key == "animations" || key == "clock24Hour" || key == "proxyEnabled")
    return value.isBool();
  if (key == "gap" || key == "panelHeight" || key == "blurRadius" ||
      key == "windowOpacity" || key == "animationDuration") {
    const double number = value.toDouble(-1);
    return value.isDouble() && std::isfinite(number) &&
           std::floor(number) == number &&
           number >= (key == "gap"             ? 4
                      : key == "panelHeight"   ? 32
                      : key == "windowOpacity" ? 60
                                               : 0) &&
           number <= (key == "gap" || key == "blurRadius" ? 32
                      : key == "panelHeight"              ? 56
                      : key == "windowOpacity"            ? 100
                                                          : 600);
  }
  return false;
}
} // namespace

QJsonObject desktopPreferences() {
  auto result = defaults();
  QSettings settings;
  for (auto it = result.begin(); it != result.end(); ++it) {
    auto value = QJsonValue::fromVariant(
        settings.value("desktop/" + it.key(), it.value().toVariant()));
    if (value.isString() && it.value().isBool()) {
      if (value.toString() == "true")
        value = true;
      else if (value.toString() == "false")
        value = false;
    } else if (value.isString() && it.value().isDouble()) {
      bool ok = false;
      const double number = value.toString().toDouble(&ok);
      if (ok)
        value = number;
    }
    if (valid(it.key(), value))
      it.value() = value;
  }

  QJsonArray wallpaperHistory;
  const auto history =
      settings.value("appearance/wallpaperHistory").toStringList();
  for (const auto &path : history)
    wallpaperHistory.append(path);
  result.insert("wallpaperHistory", wallpaperHistory);
  result.insert(
      "wallpaperRevision",
      static_cast<qint64>(
          settings.value("appearance/wallpaperRevision", 0).toULongLong()));
  return result;
}

bool updateDesktopPreferences(const QJsonObject &changes, QString *error) {
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (!valid(it.key(), it.value())) {
      if (error)
        *error = "Invalid desktop preference: " + it.key();
      return false;
    }
  }
  QSettings settings;
  for (auto it = changes.begin(); it != changes.end(); ++it)
    settings.setValue("desktop/" + it.key(), it.value().toVariant());
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = "Could not save desktop preferences.";
    return false;
  }
  return true;
}
bool setupComplete() {
  return qEnvironmentVariableIntValue("LUDASH_SKIP_SETUP") == 1 ||
         QSettings().value("session/setupComplete", false).toBool();
}
void setSetupComplete(bool complete) {
  QSettings settings;
  settings.setValue("session/setupComplete", complete);
  settings.sync();
}
} // namespace LunaDash
