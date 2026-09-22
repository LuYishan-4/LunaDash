#include "shell/modules/ShellModules.hpp"
#include "shell/modules/ShellModuleSchema.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
int qInitResources_module_templates();
namespace LunaDash {
ShellModules::ShellModules(QObject *parent) : QObject(parent) {
  ::qInitResources_module_templates();
  const auto base =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
      "/LuDash";
  QDir().mkpath(base);
  path_ = base + "/shell-modules.json";
  codeRoot_ = base + "/modules";
  document_ = defaultModuleDocument();
  debounce_.setSingleShot(true);
  debounce_.setInterval(100);
  connect(&debounce_, &QTimer::timeout, this, &ShellModules::reload);
  const auto changed = [this](const QString &path) {
    if (path.startsWith(codeRoot_ + "/") || path == codeRoot_)
      codeChanged_ = true;
    debounce_.start();
  };
  connect(&watcher_, &QFileSystemWatcher::fileChanged, this, changed);
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, changed);
  watcher_.addPath(base);
  if (!QFileInfo::exists(path_)) {
    QString error;
    if (!apply(QJsonDocument(document_).toJson(), &error))
      status_ = error;
  }
  reload();
}
QString ShellModules::entrypoint(const QString &relative) const {
  const auto root = QFileInfo(codeRoot_).canonicalFilePath();
  const QFileInfo file(codeRoot_ + "/" + relative);
  const auto path = file.canonicalFilePath();
  if (root.isEmpty() || !file.isFile() || !file.isReadable() ||
      file.size() > 256 * 1024 || !path.startsWith(root + "/"))
    return {};
  return path;
}
bool ShellModules::checkEntrypoints(const QJsonObject &document,
                                    QString *error) const {
  const auto modules = document.value("modules").toObject();
  for (auto it = modules.begin(); it != modules.end(); ++it) {
    const auto custom = it.value().toObject().value("custom").toObject();
    if (custom.value("enabled").toBool() &&
        entrypoint(custom.value("entry").toString()).isEmpty()) {
      if (error)
        *error = "Custom entry is missing, outside the module directory, or "
                 "larger than 256 KiB: " +
                 it.key();
      return false;
    }
  }
  return true;
}
bool ShellModules::validate(const QByteArray &text, QString *error) {
  QJsonObject document;
  const bool valid = validateModuleDocument(text, &document, error) &&
                     checkEntrypoints(document, error);
  status_ = valid   ? "Module JSON is valid."
            : error ? *error
                    : "Invalid module JSON.";
  return valid;
}
bool ShellModules::apply(const QByteArray &text, QString *error) {
  QJsonObject document;
  if (!validateModuleDocument(text, &document, error) ||
      !checkEntrypoints(document, error))
    return false;
  if (QFileInfo(path_).isSymLink()) {
    if (error)
      *error = "Refusing to overwrite a symlinked module configuration.";
    return false;
  }
  QSaveFile file(path_);
  const auto bytes = QJsonDocument(document).toJson(QJsonDocument::Indented);
  if (!file.open(QIODevice::WriteOnly) ||
      !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
      file.write(bytes) != bytes.size() || !file.commit()) {
    if (error)
      *error = "Could not save module configuration atomically.";
    return false;
  }
  status_ = "Module configuration saved.";
  reload();
  return true;
}
bool ShellModules::reset(QString *error) {
  if (!apply(QJsonDocument(defaultModuleDocument()).toJson(), error))
    return false;
  if (!setCodeTrusted(false, error))
    return false;
  errors_ = {};
  status_ = "Built-in modules restored. Custom files were preserved.";
  return true;
}
bool ShellModules::setCodeTrusted(bool trusted, QString *error) {
  if (trusted && !checkEntrypoints(document_, error))
    return false;
  QSettings settings;
  settings.setValue("modules/allowCustomCode", trusted);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = "Could not save custom-code preference.";
    return false;
  }
  status_ = trusted ? "Custom QML is allowed with your user permissions."
                    : "Custom QML is disabled.";
  return true;
}
void ShellModules::watchEntries() {
  if (!watcher_.files().isEmpty())
    watcher_.removePaths(watcher_.files());
  if (!watcher_.directories().isEmpty())
    watcher_.removePaths(watcher_.directories());
  watcher_.addPath(QFileInfo(path_).absolutePath());
  if (QFileInfo::exists(path_) && !watcher_.files().contains(path_))
    watcher_.addPath(path_);
  if (QFileInfo::exists(codeRoot_) &&
      !watcher_.directories().contains(codeRoot_))
    watcher_.addPath(codeRoot_);
  const auto modules = document_.value("modules").toObject();
  for (auto it = modules.begin(); it != modules.end(); ++it) {
    const auto path = entrypoint(it.value()
                                     .toObject()
                                     .value("custom")
                                     .toObject()
                                     .value("entry")
                                     .toString());
    if (path.isEmpty())
      continue;
    if (!watcher_.files().contains(path))
      watcher_.addPath(path);
    const auto directory = QFileInfo(path).absolutePath();
    if (!watcher_.directories().contains(directory))
      watcher_.addPath(directory);
  }
}
void ShellModules::reload() {
  QJsonObject next = defaultModuleDocument();
  QString error;
  if (QFileInfo::exists(path_)) {
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly) ||
        !validateModuleDocument(file.read(16385), &next, &error) ||
        !checkEntrypoints(next, &error)) {
      status_ = error.isEmpty() ? "Could not read module configuration; "
                                  "previous settings are retained."
                                : error;
      watchEntries();
      return;
    }
  }
  if (document_ != next || codeChanged_) {
    document_ = next;
    revision_ = (revision_ + 1) % 1000000;
    errors_ = {};
    emit changed();
  }
  codeChanged_ = false;
  watchEntries();
}
QJsonObject ShellModules::snapshot() const {
  auto effective = document_.value("modules").toObject();
  const bool trusted =
      QSettings().value("modules/allowCustomCode", false).toBool();
  for (auto it = effective.begin(); it != effective.end(); ++it) {
    auto module = it.value().toObject();
    auto custom = module.value("custom").toObject();
    const auto path = entrypoint(custom.value("entry").toString());
    custom["source"] =
        trusted && custom.value("enabled").toBool() && !path.isEmpty()
            ? QUrl::fromLocalFile(path).toString()
            : QString();
    module["custom"] = custom;
    it.value() = module;
  }
  return {{"document", document_}, {"modules", effective},
          {"descriptors", shellModuleDescriptors()},
          {"path", path_},         {"codeRoot", codeRoot_},
          {"trusted", trusted},    {"revision", revision_},
          {"status", status_},     {"errors", errors_}};
}
void ShellModules::reportError(const QString &id, const QString &error) {
  if (shellModuleIds().contains(id))
    errors_[id] = error.left(512);
}
int ShellModules::panelExtent(int fallbackHeight) const {
  const auto panel =
      document_.value("modules").toObject().value("panel").toObject();
  if (!panel.value("enabled").toBool())
    return 0;
  const auto style = panel.value("style").toObject();
  const int height = style.value("height").toInt();
  return (height ? height : fallbackHeight) + style.value("margin").toInt() * 2;
}
bool ShellModules::panelAtBottom() const {
  return document_.value("modules")
             .toObject()
             .value("panel")
             .toObject()
             .value("style")
             .toObject()
             .value("edge")
             .toString() == "bottom";
}
bool ShellModules::installTemplate(const QString &id, QString *error) {
  if (!shellModuleDescriptor(id).value("template").toBool()) {
    if (error)
      *error = "Selected module has no installable template.";
    return false;
  }
  const auto directory = codeRoot_ + "/" + id;
  const auto destination = directory + "/Main.qml";
  if (!QDir().mkpath(directory) ||
      !QFileInfo(directory).canonicalFilePath().startsWith(
          QFileInfo(codeRoot_).canonicalFilePath() + "/")) {
    if (error)
      *error = "Invalid template directory.";
    return false;
  }
  QFile source(":/LuDash/data/modules/templates/" + id + "/Main.qml");
  QFile target(destination);
  if (!source.open(QIODevice::ReadOnly) ||
      !target.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
    if (error)
      *error = "Template already exists or cannot be created. Existing code is "
               "never overwritten.";
    return false;
  }
  const auto bytes = source.readAll();
  if (!target.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
      target.write(bytes) != bytes.size() || !target.flush()) {
    target.close();
    target.remove();
    if (error)
      *error = "Could not write template.";
    return false;
  }
  status_ = "Template created: " + destination;
  watchEntries();
  return true;
}
} // namespace LunaDash
