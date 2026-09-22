#include "compositor/session/LaunchPolicy.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace LunaDash {
namespace {
QString normalizedDesktopId(QString value) {
  value = QFileInfo(value).fileName();
  if (value.endsWith(".desktop", Qt::CaseInsensitive))
    value.chop(8);
  return value;
}

QString flatpakId(const QStringList &command) {
  if (command.isEmpty() ||
      QFileInfo(command.first()).fileName().compare("flatpak",
                                                    Qt::CaseInsensitive) != 0)
    return {};

  const int run = command.indexOf("run");
  if (run < 0)
    return {};

  // Flatpak application IDs are reverse-DNS-style identifiers. Requiring at
  // least two dots avoids treating option values such as an architecture or
  // branch as the application identity.
  static const QRegularExpression idPattern(
      "^[A-Za-z][A-Za-z0-9_-]*(?:\\.[A-Za-z0-9_-]+){2,}$");
  for (int i = run + 1; i < command.size(); ++i) {
    const auto value = command[i];
    if (value == "--")
      continue;
    if (value.startsWith('-'))
      continue;
    if (idPattern.match(value).hasMatch())
      return value;
  }
  return {};
}

QJsonArray rulesFrom(QIODevice &device) {
  const auto bytes = device.read(65537);
  if (bytes.size() > 65536)
    return {};
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return {};
  const auto root = document.object();
  if (root.value("schemaVersion").toInt() != 1 ||
      !root.value("applications").isArray())
    return {};
  return root.value("applications").toArray();
}

bool listContains(const QJsonValue &value, const QString &needle,
                  bool desktopId = false) {
  if (needle.isEmpty() || !value.isArray())
    return false;
  for (const auto &entry : value.toArray()) {
    if (!entry.isString())
      continue;
    const auto candidate =
        desktopId ? normalizedDesktopId(entry.toString()) : entry.toString();
    if (candidate.compare(needle, Qt::CaseInsensitive) == 0)
      return true;
  }
  return false;
}

void applyRules(const QJsonArray &rules, const LaunchIdentity &identity,
                LaunchCapabilities *capabilities) {
  for (const auto &value : rules) {
    if (!value.isObject())
      continue;
    const auto rule = value.toObject();
    const bool matches =
        listContains(rule.value("desktopIds"), identity.desktopId, true) ||
        listContains(rule.value("flatpakIds"), identity.flatpakId) ||
        listContains(rule.value("executables"), identity.executable);
    if (!matches)
      continue;
    const auto declared = rule.value("capabilities").toArray();
    for (const auto &capability : declared)
      if (capability.toString() == "x11-helper")
        capabilities->x11Helper = true;
  }
}
} // namespace

LaunchIdentity identifyLaunch(const QString &desktopId,
                              const QStringList &command) {
  LaunchIdentity identity;
  identity.desktopId = normalizedDesktopId(desktopId);
  if (!command.isEmpty())
    identity.executable = QFileInfo(command.first()).fileName();
  identity.flatpakId = flatpakId(command);
  return identity;
}

LaunchCapabilities resolveLaunchCapabilities(const QString &desktopId,
                                             const QStringList &command) {
  const auto identity = identifyLaunch(desktopId, command);
  LaunchCapabilities capabilities;

  QFile bundled(":/LunaDash/session/launch-capabilities.json");
  if (bundled.open(QIODevice::ReadOnly))
    applyRules(rulesFrom(bundled), identity, &capabilities);

  const auto configRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  QFile user(QDir(configRoot).filePath("lunadash/launch-capabilities.json"));
  if (user.open(QIODevice::ReadOnly))
    applyRules(rulesFrom(user), identity, &capabilities);

  return capabilities;
}

} // namespace LunaDash
