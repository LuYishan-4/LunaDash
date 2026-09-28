#include "shell/modules/ShellModules.hpp"
#include "shell/modules/ShellModuleSchema.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
int qInitResources_module_templates();
namespace LunaDash {
ShellModules::ShellModules(QObject *parent) : QObject(parent) {
  ::qInitResources_module_templates();
  const auto base =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
      "/LuDash";
  QDir().mkpath(base);
  path_ = base + "/shell-modules.json";
  document_ = defaultModuleDocument();
  debounce_.setSingleShot(true);
  debounce_.setInterval(100);
  connect(&debounce_, &QTimer::timeout, this, &ShellModules::reload);
  const auto changed = [this](const QString &) { debounce_.start(); };
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
bool ShellModules::validate(const QByteArray &text, QString *error) {
  QJsonObject document;
  const bool valid = validateModuleDocument(text, &document, error);
  status_ = valid   ? "Module JSON is valid."
            : error ? *error
                    : "Invalid module JSON.";
  return valid;
}
bool ShellModules::apply(const QByteArray &text, QString *error) {
  QJsonObject document;
  if (!validateModuleDocument(text, &document, error))
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
  errors_ = {};
  status_ = "Built-in modules restored. Legacy files were preserved.";
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
}
void ShellModules::reload() {
  QJsonObject next = defaultModuleDocument();
  QString error;
  if (QFileInfo::exists(path_)) {
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly) ||
        !validateModuleDocument(file.read(16385), &next, &error)) {
      status_ = error.isEmpty() ? "Could not read module configuration; "
                                  "previous settings are retained."
                                : error;
      watchEntries();
      return;
    }
  }
  if (document_ != next) {
    document_ = next;
    revision_ = (revision_ + 1) % 1000000;
    errors_ = {};
    emit changed();
  }
  watchEntries();
}
QJsonObject ShellModules::snapshot() const {
  return {{"document", document_},
          {"modules", document_.value("modules")},
          {"descriptors", shellModuleDescriptors()},
          {"path", path_}, {"revision", revision_},
          {"status", status_}, {"errors", errors_}};
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
  return qMax(24, height ? height : fallbackHeight) +
         2 * style.value("margin").toInt();
}
QString ShellModules::panelEdge() const {
  const auto edge =
      document_.value("modules")
          .toObject()
          .value("panel")
          .toObject()
          .value("style")
          .toObject()
          .value("edge")
          .toString("top");
  return QStringList{"top", "bottom", "left", "right"}.contains(edge)
             ? edge
             : QStringLiteral("top");
}
bool ShellModules::panelAtBottom() const { return panelEdge() == "bottom"; }
} // namespace LunaDash
