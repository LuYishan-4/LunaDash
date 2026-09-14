#include <LuDash/default_applications/DefaultApplications.h>
#include <QFileInfo>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
namespace LuDash {
namespace {
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
  for (const auto &role : {"terminal", "files"}) {
    const auto value = QJsonValue::fromVariant(
        QSettings().value(QString("defaultApps/") + role, QStringList{}));
    result[role] = validCommand(value) ? value : QJsonValue(QJsonArray{});
  }
  return result;
}
bool setDefaultApplications(const QJsonObject &changes, QString *error) {
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if ((it.key() != "terminal" && it.key() != "files") ||
        !validCommand(it.value())) {
      if (error)
        *error =
            "Defaults require terminal/files argument arrays (empty selects "
            "LuDash), at most 24 strings, no recursive LuDash launcher.";
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
  if (role != "terminal" && role != "files") {
    if (error)
      *error = "Unknown application role.";
    return {};
  }
  const auto configured = defaultApplications().value(role).toArray();
  QStringList result;
  for (const auto &argument : configured)
    result << argument.toString();
  if (!result.isEmpty())
    return result;
  // Both built-in defaults are shipped inside LunaDash: Files and the bundled
  // Fish terminal. An empty command asks the compositor to open the matching
  // built-in application instead of an external executable.
  if (role == "files" || role == "terminal")
    return {};
  return {};
}
} // namespace LuDash
