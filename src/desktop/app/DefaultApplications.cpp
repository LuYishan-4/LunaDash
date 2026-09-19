#include "desktop/app/DefaultApplications.hpp"
#include "desktop/browser/Browser.hpp"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
namespace LunaDash {
namespace {
QStringList konsoleCommandForProfile(const QString &configDirectory,
                                     const QString &launcher,
                                     const QStringList &launcherArguments) {
  const auto config = configDirectory + QStringLiteral("/konsolerc");
  const auto profile =
      QSettings(config, QSettings::IniFormat)
          .value(QStringLiteral("Desktop Entry/DefaultProfile"))
          .toString()
          .trimmed();
  if (profile.isEmpty())
    return {};

  const auto profilePath =
      configDirectory + QStringLiteral("/../data/konsole/") + profile;
  if (!QFileInfo::exists(profilePath))
    return {};

  QStringList command{launcher};
  command += launcherArguments;
  command << "--profile" << QFileInfo(profile).completeBaseName()
          << "--separate";
  return command;
}

QStringList userKonsoleCommand() {
  const auto configDirectory =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  const auto flatpakConfig =
      QDir::homePath() + QStringLiteral("/.var/app/org.kde.konsole/config");
  if (!QStandardPaths::findExecutable("flatpak").isEmpty()) {
    const auto flatpakCommand = konsoleCommandForProfile(
        flatpakConfig, "flatpak", {"run", "org.kde.konsole"});
    if (!flatpakCommand.isEmpty())
      return flatpakCommand;
  }

  const auto nativeProfile =
      QSettings(configDirectory + QStringLiteral("/konsolerc"),
                QSettings::IniFormat)
          .value(QStringLiteral("Desktop Entry/DefaultProfile"))
          .toString()
          .trimmed();
  if (nativeProfile.isEmpty())
    return {"konsole", "--separate"};
  const auto nativeProfilePath =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      QStringLiteral("/konsole/") + nativeProfile;
  if (QFileInfo::exists(nativeProfilePath))
    return {"konsole", "--profile", QFileInfo(nativeProfile).completeBaseName(),
            "--separate"};
  return {"konsole", "--profile", nativeProfile, "--separate"};
}

QStringList normalizeKonsoleCommand(const QStringList &command) {
  if (command.isEmpty() ||
      QFileInfo(command.first())
              .fileName()
              .compare("konsole", Qt::CaseInsensitive) != 0 ||
      command.contains("--profile") || command.contains("--separate") ||
      command.contains("-e"))
    return command;
  auto result = userKonsoleCommand();
  result += command.mid(1);
  return result;
}

bool validCommand(const QJsonValue &value) {
  if (!value.isArray() || value.toArray().size() > 24)
    return false;
  const auto arguments = value.toArray();
  for (const auto &argument : arguments)
    if (!argument.isString() || argument.toString().size() > 1024 ||
        argument.toString().contains(QChar(0)))
      return false;
  if (arguments.isEmpty())
    return true;
  const auto program = arguments.first().toString();
  return !program.isEmpty() && !program.startsWith('-') &&
         QFileInfo(program).fileName() != "ludash-desktop" &&
         QFileInfo(program).fileName() != "ludashctl";
}
} // namespace
QJsonObject defaultApplications() {
  QJsonObject result;
  for (const auto &role : {"terminal", "files", "browser"}) {
    const auto value = QJsonValue::fromVariant(
        QSettings().value(QString("defaultApps/") + role, QStringList{}));
    result[role] = validCommand(value) ? value : QJsonValue(QJsonArray{});
  }
  return result;
}
bool setDefaultApplications(const QJsonObject &changes, QString *error) {
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if ((it.key() != "terminal" && it.key() != "files" &&
         it.key() != "browser") ||
        !validCommand(it.value())) {
      if (error)
        *error =
            "Defaults require terminal/files/browser argument arrays (empty "
            "selects the LunaDash default), at most 24 strings, no recursive "
            "LunaDash launcher.";
      return false;
    }
    const auto command = it.value().toArray();
    if (!command.isEmpty() &&
        QStandardPaths::findExecutable(command.first().toString()).isEmpty()) {
      if (error)
        *error = "Application executable was not found.";
      return false;
    }
  }
  QSettings settings;
  for (auto it = changes.begin(); it != changes.end(); ++it)
    settings.setValue("defaultApps/" + it.key(), it.value().toVariant());
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = "Could not save default applications.";
    return false;
  }
  return true;
}
QStringList defaultApplicationCommand(const QString &role, QString *error) {
  if (role != "terminal" && role != "files" && role != "browser") {
    if (error)
      *error = "Unknown application role.";
    return {};
  }
  const auto configured = defaultApplications().value(role).toArray();
  QStringList result;
  for (const auto &argument : configured)
    result << argument.toString();
  if (!result.isEmpty())
    return role == "terminal" ? normalizeKonsoleCommand(result) : result;
  if (role == "terminal")
    return userKonsoleCommand();
  if (role == "files")
    return {};
  if (role == "browser") {
    const auto browser = Browser::defaultCommand();
    if (browser.isEmpty() && error)
      *error = "Google Chrome or Chromium is not installed.";
    return browser;
  }
  return {};
}

QStringList konsoleCommand() { return userKonsoleCommand(); }
} // namespace LunaDash
