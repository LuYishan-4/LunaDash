#include "config/Localization/Localization.hpp"
#include "desktop/DesktopTheme/DesktopTheme.hpp"

#include <QApplication>
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>

namespace {
QString optionPath(const QVariantMap &options, const QString &key) {
  QByteArray bytes = options.value(key).toByteArray();
  if (!bytes.isEmpty() && bytes.endsWith('\0')) bytes.chop(1);
  if (!bytes.isEmpty()) return QFile::decodeName(bytes);
  return {};
}

QString initialDirectory(const QVariantMap &options) {
  auto path = optionPath(options, QStringLiteral("current_folder"));
  if (path.isEmpty()) {
    const auto file = optionPath(options, QStringLiteral("current_file"));
    if (!file.isEmpty()) path = QFileInfo(file).absolutePath();
  }
  return path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                        : path;
}

QVariantMap uriResults(const QStringList &paths) {
  QStringList uris;
  for (const auto &path : paths) {
    const QFileInfo info(path);
    if (info.exists() || QFileInfo(info.absolutePath()).isDir())
      uris << QUrl::fromLocalFile(info.absoluteFilePath()).toString(QUrl::FullyEncoded);
  }
  return {{QStringLiteral("uris"), uris}};
}

void styleDialog(QFileDialog &dialog) {
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  dialog.setOption(QFileDialog::DontResolveSymlinks, false);
  LuDash::watchDesktopTheme(&dialog);
}
} // namespace

class FileChooserPortal final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")

public slots:
  uint OpenFile(const QDBusObjectPath &, const QString &, const QString &,
                const QString &title, const QVariantMap &options,
                QVariantMap &results) {
    QFileDialog dialog(nullptr, title.isEmpty() ? LuDash::translate("Open file") : title,
                       initialDirectory(options));
    styleDialog(dialog);
    const bool directory = options.value(QStringLiteral("directory"), false).toBool();
    const bool multiple = options.value(QStringLiteral("multiple"), false).toBool();
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(directory ? QFileDialog::Directory
                                 : (multiple ? QFileDialog::ExistingFiles
                                             : QFileDialog::ExistingFile));
    if (directory) dialog.setOption(QFileDialog::ShowDirsOnly, true);
    const auto accept = options.value(QStringLiteral("accept_label")).toString();
    if (!accept.isEmpty()) dialog.setLabelText(QFileDialog::Accept, accept);
    if (dialog.exec() != QDialog::Accepted) {
      results.clear();
      return 1;
    }
    results = uriResults(dialog.selectedFiles());
    return 0;
  }

  uint SaveFile(const QDBusObjectPath &, const QString &, const QString &,
                const QString &title, const QVariantMap &options,
                QVariantMap &results) {
    QFileDialog dialog(nullptr, title.isEmpty() ? LuDash::translate("Save file") : title,
                       initialDirectory(options));
    styleDialog(dialog);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    const auto currentName = options.value(QStringLiteral("current_name")).toString();
    if (!currentName.isEmpty()) dialog.selectFile(currentName);
    const auto accept = options.value(QStringLiteral("accept_label")).toString();
    if (!accept.isEmpty()) dialog.setLabelText(QFileDialog::Accept, accept);
    if (dialog.exec() != QDialog::Accepted) {
      results.clear();
      return 1;
    }
    results = uriResults(dialog.selectedFiles());
    return 0;
  }

  uint SaveFiles(const QDBusObjectPath &, const QString &, const QString &,
                 const QString &title, const QVariantMap &options,
                 QVariantMap &results) {
    QFileDialog dialog(nullptr, title.isEmpty() ? LuDash::translate("Choose folder") : title,
                       initialDirectory(options));
    styleDialog(dialog);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
      results.clear();
      return 1;
    }
    const QDir directory(dialog.selectedFiles().first());
    QStringList paths;
    const auto files = options.value(QStringLiteral("files")).toList();
    for (const auto &value : files) {
      QByteArray name = value.toByteArray();
      if (!name.isEmpty() && name.endsWith('\0')) name.chop(1);
      if (!name.isEmpty()) paths << directory.filePath(QFile::decodeName(name));
    }
    results = uriResults(paths);
    return 0;
  }
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("LunaDash File Chooser"));
  app.setOrganizationName(QStringLiteral("LunaDash"));
  LuDash::initializeLocalization(app);

  auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) return 2;
  if (!bus.registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.lunadash")))
    return 3;

  FileChooserPortal portal;
  if (!bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), &portal,
                          QDBusConnection::ExportAllSlots))
    return 4;
  return app.exec();
}

#include "portal_main.moc"
