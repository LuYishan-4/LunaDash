#include "service/portal/FileChooserPortal.hpp"
#include "config/localization/Localization.hpp"
#include "service/portal/FileChooserOptions.hpp"
#include "service/portal/FilePickerDialog.hpp"
#include "service/portal/PortalRequest.hpp"
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

namespace LunaDash {
uint FileChooserPortal::OpenFile(const QDBusObjectPath &handle,
                                 const QString &, const QString &,
                                 const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results) {
  const bool directory = options.value("directory").toBool();
  FilePickerDialog picker(
      directory ? FilePickerDialog::Mode::Directory
                : FilePickerDialog::Mode::Open,
      title.isEmpty() ? translate(directory ? "Choose folder" : "Open file")
                      : title,
      options);
  configureFilePicker(picker, options);
  results.clear();
  PortalRequest request(&picker);
  auto bus = QDBusConnection::sessionBus();
  const bool exposeRequest = calledFromDBus();
  if (exposeRequest &&
      (!bus.isConnected() ||
       !bus.registerObject(handle.path(), &request,
                           QDBusConnection::ExportAllSlots)))
    return 2;
  const int dialogResult = picker.exec();
  if (exposeRequest)
    bus.unregisterObject(handle.path());
  if (dialogResult != QDialog::Accepted)
    return 1;
  results = filePickerResults(picker, options);
  return 0;
}

uint FileChooserPortal::SaveFile(const QDBusObjectPath &handle,
                                 const QString &, const QString &,
                                 const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results) {
  FilePickerDialog picker(FilePickerDialog::Mode::Save,
                          title.isEmpty() ? translate("Save file") : title,
                          options);
  configureFilePicker(picker, options);
  results.clear();
  PortalRequest request(&picker);
  auto bus = QDBusConnection::sessionBus();
  const bool exposeRequest = calledFromDBus();
  if (exposeRequest &&
      (!bus.isConnected() ||
       !bus.registerObject(handle.path(), &request,
                           QDBusConnection::ExportAllSlots)))
    return 2;
  const int dialogResult = picker.exec();
  if (exposeRequest)
    bus.unregisterObject(handle.path());
  if (dialogResult != QDialog::Accepted)
    return 1;
  results = filePickerResults(picker, options);
  return 0;
}

uint FileChooserPortal::SaveFiles(const QDBusObjectPath &handle,
                                  const QString &, const QString &,
                                  const QString &title,
                                  const QVariantMap &options,
                                  QVariantMap &results) {
  results.clear();
  const auto names = portalSaveFileNames(options);
  if (names.isEmpty())
    return 2;
  auto folderOptions = options;
  folderOptions["multiple"] = false;
  FilePickerDialog picker(FilePickerDialog::Mode::Directory,
                          title.isEmpty() ? translate("Choose folder") : title,
                          folderOptions);
  configureFilePicker(picker, folderOptions);
  PortalRequest request(&picker);
  auto bus = QDBusConnection::sessionBus();
  const bool exposeRequest = calledFromDBus();
  if (exposeRequest &&
      (!bus.isConnected() ||
       !bus.registerObject(handle.path(), &request,
                           QDBusConnection::ExportAllSlots)))
    return 2;
  const int dialogResult = picker.exec();
  if (exposeRequest)
    bus.unregisterObject(handle.path());
  if (dialogResult != QDialog::Accepted)
    return 1;
  const QDir directory(picker.selectedPaths().first());
  if (!QFileInfo(directory.absolutePath()).isWritable())
    return 2;
  QStringList uris;
  QSet<QString> reserved;
  for (const auto &name : names) {
    QString unique = name;
    int suffix = 1;
    while (QFileInfo::exists(directory.filePath(unique)) ||
           reserved.contains(unique)) {
      const QFileInfo info(name);
      unique = info.completeBaseName() + QString(" (%1)").arg(suffix++);
      if (!info.suffix().isEmpty())
        unique += "." + info.suffix();
    }
    reserved.insert(unique);
    uris.append(QUrl::fromLocalFile(directory.filePath(unique))
                    .toString(QUrl::FullyEncoded));
  }
  results = filePickerResults(picker, folderOptions);
  results["uris"] = uris;
  return 0;
}
} // namespace LunaDash
