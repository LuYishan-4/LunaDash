#include "service/portal/FileChooserPortal.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/theme/DesktopTheme.hpp"

#include <QApplication>
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>
#include <QVariantMap>
#include <QWidget>

namespace LunaDash {
namespace {
QString optionPath(const QVariantMap &options, const QString &key) {
  QByteArray bytes = options.value(key).toByteArray();
  if (!bytes.isEmpty() && bytes.endsWith('\0'))
    bytes.chop(1);
  if (!bytes.isEmpty())
    return QFile::decodeName(bytes);
  return {};
}

QString initialDirectory(const QVariantMap &options) {
  auto path = optionPath(options, QStringLiteral("current_folder"));
  if (path.isEmpty()) {
    const auto file = optionPath(options, QStringLiteral("current_file"));
    if (!file.isEmpty())
      path = QFileInfo(file).absolutePath();
  }
  return path.isEmpty()
             ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
             : path;
}

QString localPath(QString value) {
  value = value.trimmed();
  const QUrl url(value);
  if (url.isValid() && url.isLocalFile())
    value = url.toLocalFile();
  if (!QFileInfo(value).isAbsolute())
    return {};
  return QDir::cleanPath(value);
}

QVariantMap uriResults(const QStringList &paths) {
  QStringList uris;
  for (const auto &path : paths) {
    const QFileInfo info(path);
    if (info.exists() || QFileInfo(info.absolutePath()).isDir())
      uris << QUrl::fromLocalFile(info.absoluteFilePath())
                  .toString(QUrl::FullyEncoded);
  }
  return {{QStringLiteral("uris"), uris}};
}

void installLocationBar(QFileDialog &dialog, const QString &initial) {
  auto *container = new QWidget(&dialog);
  auto *layout = new QHBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto *label = new QLabel(LunaDash::translate("Location"), container);
  label->setObjectName(QStringLiteral("muted"));
  auto *field = new QLineEdit(container);
  field->setObjectName(QStringLiteral("portalLocation"));
  field->setClearButtonEnabled(true);
  field->setPlaceholderText(
      LunaDash::translate("Paste an absolute path or file:// URL"));
  field->setText(QDir::cleanPath(initial));
  layout->addWidget(label);
  layout->addWidget(field, 1);

  if (dialog.layout())
    dialog.layout()->addWidget(container);

  QObject::connect(&dialog, &QFileDialog::directoryEntered, field,
                   [field](const QString &path) { field->setText(path); });
  QObject::connect(field, &QLineEdit::returnPressed, &dialog,
                   [&dialog, field] {
                     const QString path = localPath(field->text());
                     if (path.isEmpty()) {
                       field->setProperty("invalidPath", true);
                       field->style()->unpolish(field);
                       field->style()->polish(field);
                       return;
                     }
                     field->setProperty("invalidPath", false);
                     const QFileInfo info(path);
                     if (info.isDir()) {
                       dialog.setDirectory(path);
                     } else {
                       const QString parent = info.absolutePath();
                       if (QFileInfo(parent).isDir())
                         dialog.setDirectory(parent);
                       dialog.selectFile(path);
                     }
                   });
}

void styleDialog(QFileDialog &dialog, const QString &initial) {
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  dialog.setOption(QFileDialog::DontResolveSymlinks, false);
  dialog.resize(980, 650);
  dialog.setMinimumSize(760, 480);
  LunaDash::watchDesktopTheme(&dialog);
  installLocationBar(dialog, initial);
}
} // namespace

uint FileChooserPortal::OpenFile(const QDBusObjectPath &, const QString &,
                                 const QString &, const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results) {
  const QString initial = initialDirectory(options);
  QFileDialog dialog(nullptr,
                     title.isEmpty() ? LunaDash::translate("Open file") : title,
                     initial);
  styleDialog(dialog, initial);
  const bool directory =
      options.value(QStringLiteral("directory"), false).toBool();
  const bool multiple =
      options.value(QStringLiteral("multiple"), false).toBool();
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  dialog.setFileMode(directory ? QFileDialog::Directory
                               : (multiple ? QFileDialog::ExistingFiles
                                           : QFileDialog::ExistingFile));
  if (directory)
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
  const auto accept = options.value(QStringLiteral("accept_label")).toString();
  if (!accept.isEmpty())
    dialog.setLabelText(QFileDialog::Accept, accept);
  if (dialog.exec() != QDialog::Accepted) {
    results.clear();
    return 1;
  }
  results = uriResults(dialog.selectedFiles());
  return 0;
}

uint FileChooserPortal::SaveFile(const QDBusObjectPath &, const QString &,
                                 const QString &, const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results) {
  const QString initial = initialDirectory(options);
  QFileDialog dialog(nullptr,
                     title.isEmpty() ? LunaDash::translate("Save file") : title,
                     initial);
  styleDialog(dialog, initial);
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setFileMode(QFileDialog::AnyFile);
  const auto currentName =
      options.value(QStringLiteral("current_name")).toString();
  if (!currentName.isEmpty())
    dialog.selectFile(currentName);
  const auto accept = options.value(QStringLiteral("accept_label")).toString();
  if (!accept.isEmpty())
    dialog.setLabelText(QFileDialog::Accept, accept);
  if (dialog.exec() != QDialog::Accepted) {
    results.clear();
    return 1;
  }
  results = uriResults(dialog.selectedFiles());
  return 0;
}

uint FileChooserPortal::SaveFiles(const QDBusObjectPath &, const QString &,
                                  const QString &, const QString &title,
                                  const QVariantMap &options,
                                  QVariantMap &results) {
  const QString initial = initialDirectory(options);
  QFileDialog dialog(
      nullptr, title.isEmpty() ? LunaDash::translate("Choose folder") : title,
      initial);
  styleDialog(dialog, initial);
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
    if (!name.isEmpty() && name.endsWith('\0'))
      name.chop(1);
    if (!name.isEmpty())
      paths << directory.filePath(QFile::decodeName(name));
  }
  results = uriResults(paths);
  return 0;
}
} // namespace LunaDash
