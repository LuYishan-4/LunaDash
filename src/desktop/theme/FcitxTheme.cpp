#include "desktop/theme/FcitxTheme.hpp"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

int qInitResources_application_themes();
namespace LunaDash {
namespace {
bool save(const QString &path, const QByteArray &bytes, QString *error) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile file(path);
  if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() &&
      file.commit())
    return true;
  if (error)
    *error = file.errorString();
  return false;
}
} // namespace
bool synchronizeFcitxTheme(const QJsonObject &palette, QString *error) {
  ::qInitResources_application_themes();
  const auto directory =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      "/fcitx5/themes/lunadash-mellow";
  const QMap<QString, QString> roles{
      {"on_surface", "text"}, {"on_primary", "accentInk"},
      {"primary", "accent"},  {"surface_container_lowest", "background"},
      {"outline", "border"},  {"outline_variant", "hairline"}};
  for (const auto &name : {"theme.conf", "panel.svg", "highlight.svg"}) {
    QFile source(QStringLiteral(":/LunaDash/theme/nyxmellow/") + name);
    if (!source.open(QIODevice::ReadOnly)) {
      if (error)
        *error = "Missing bundled input-method theme.";
      return false;
    }
    auto content = QString::fromUtf8(source.readAll());
    for (auto it = roles.begin(); it != roles.end(); ++it)
      content.replace("{{ colors." + it.key() + ".default.hex }}",
                      palette.value(it.value()).toString());
    if (content.contains("{{")) {
      if (error)
        *error = "Unresolved input-method theme color.";
      return false;
    }
    if (!save(directory + "/" + name, content.toUtf8(), error))
      return false;
  }
  const auto path =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
      "/fcitx5/conf/classicui.conf";
  QFile file(path);
  QString text;
  if (file.exists()) {
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
      if (error)
        *error = "Could not read Fcitx 5 appearance settings.";
      return false;
    }
    text = QString::fromUtf8(file.readAll());
    if (!QFileInfo::exists(path + ".lunadash-backup") &&
        !file.copy(path + ".lunadash-backup")) {
      if (error)
        *error = "Could not back up Fcitx 5 appearance settings.";
      return false;
    }
  }
  // Fcitx uses flat keys before any section; QSettings would add [General].
  for (const auto &key : {"Theme", "DarkTheme"}) {
    const QRegularExpression line("^" + QString(key) + "=.*$",
                                  QRegularExpression::MultilineOption);
    if (text.contains(line))
      text.replace(line, QString(key) + "=lunadash-mellow");
    else
      text.prepend(QString(key) + "=lunadash-mellow\n");
  }
  if (!save(path, text.toUtf8(), error))
    return false;
  auto message = QDBusMessage::createMethodCall(
      "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1",
      "ReloadAddonConfig");
  message.setAutoStartService(false);
  message << "classicui";
  QDBusConnection::sessionBus().asyncCall(message);
  return true;
}
} // namespace LunaDash
