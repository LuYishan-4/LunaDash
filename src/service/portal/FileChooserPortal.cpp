#include "service/portal/FileChooserPortal.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/theme/DesktopTheme.hpp"

#include <QApplication>
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

namespace LunaDash {
namespace {

class PortalTitleBar final : public QFrame {
public:
  explicit PortalTitleBar(QWidget *parent = nullptr) : QFrame(parent) {}

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton && window() &&
        window()->windowHandle())
      window()->windowHandle()->startSystemMove();
    QFrame::mousePressEvent(event);
  }
};

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

QWidget *makeLocationBar(QFileDialog &dialog, const QString &initial,
                         QWidget *parent) {
  auto *container = new QWidget(parent);
  container->setObjectName(QStringLiteral("portalLocationBar"));
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

  auto *go = new QPushButton(LunaDash::translate("Go"), container);
  go->setObjectName(QStringLiteral("accent"));
  go->setMinimumWidth(70);

  layout->addWidget(label);
  layout->addWidget(field, 1);
  layout->addWidget(go);

  const auto apply = [&dialog, field] {
    const QString path = localPath(field->text());
    if (path.isEmpty()) {
      field->setProperty("invalidPath", true);
      field->style()->unpolish(field);
      field->style()->polish(field);
      return;
    }

    field->setProperty("invalidPath", false);
    field->style()->unpolish(field);
    field->style()->polish(field);

    const QFileInfo info(path);
    if (info.isDir()) {
      dialog.setDirectory(path);
      return;
    }

    const QString parentPath = info.absolutePath();
    if (QFileInfo(parentPath).isDir())
      dialog.setDirectory(parentPath);
    dialog.selectFile(path);
  };

  QObject::connect(&dialog, &QFileDialog::directoryEntered, field,
                   [field](const QString &path) { field->setText(path); });
  QObject::connect(field, &QLineEdit::returnPressed, container, apply);
  QObject::connect(go, &QPushButton::clicked, container, apply);
  return container;
}

int execStyledDialog(QFileDialog &picker, const QString &title,
                     const QString &initial) {
  QDialog shell;
  shell.setObjectName(QStringLiteral("portalFileShell"));
  shell.setWindowTitle(title);
  shell.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  shell.setAttribute(Qt::WA_TranslucentBackground, true);
  shell.setModal(true);
  shell.setSizeGripEnabled(true);
  shell.resize(1080, 720);
  shell.setMinimumSize(820, 560);
  LunaDash::watchDesktopTheme(&shell);

  auto *outer = new QVBoxLayout(&shell);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  auto *frame = new QFrame(&shell);
  frame->setObjectName(QStringLiteral("portalFileFrame"));
  outer->addWidget(frame);

  auto *layout = new QVBoxLayout(frame);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);

  auto *header = new PortalTitleBar(frame);
  header->setObjectName(QStringLiteral("portalFileHeader"));
  header->setFixedHeight(58);
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(16, 8, 8, 8);
  headerLayout->setSpacing(10);

  auto *copy = new QVBoxLayout;
  copy->setSpacing(0);
  auto *heading = new QLabel(title, header);
  heading->setObjectName(QStringLiteral("portalFileHeading"));
  auto *caption =
      new QLabel(LunaDash::translate("LunaDash file picker"), header);
  caption->setObjectName(QStringLiteral("portalFileCaption"));
  copy->addWidget(heading);
  copy->addWidget(caption);
  headerLayout->addLayout(copy, 1);

  auto *close = new QToolButton(header);
  close->setObjectName(QStringLiteral("portalFileClose"));
  close->setText(QStringLiteral("×"));
  close->setFixedSize(36, 36);
  headerLayout->addWidget(close);
  QObject::connect(close, &QToolButton::clicked, &shell, &QDialog::reject);
  layout->addWidget(header);

  picker.setParent(frame);
  picker.setWindowFlags(Qt::Widget);
  picker.setOption(QFileDialog::DontUseNativeDialog, true);
  picker.setOption(QFileDialog::DontResolveSymlinks, false);
  picker.setObjectName(QStringLiteral("portalEmbeddedFileDialog"));
  picker.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  picker.setModal(false);
  picker.setWindowTitle(title);

  layout->addWidget(makeLocationBar(picker, initial, frame));
  layout->addWidget(&picker, 1);

  QObject::connect(&picker, &QFileDialog::accepted, &shell, &QDialog::accept);
  QObject::connect(&picker, &QFileDialog::rejected, &shell, &QDialog::reject);
  QObject::connect(&shell, &QDialog::rejected, &picker, &QFileDialog::reject);

  return shell.exec();
}

} // namespace

uint FileChooserPortal::OpenFile(const QDBusObjectPath &, const QString &,
                                 const QString &, const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results) {
  const QString initial = initialDirectory(options);
  const QString heading =
      title.isEmpty() ? LunaDash::translate("Open file") : title;

  QFileDialog dialog(nullptr, heading, initial);
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

  if (execStyledDialog(dialog, heading, initial) != QDialog::Accepted) {
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
  const QString heading =
      title.isEmpty() ? LunaDash::translate("Save file") : title;

  QFileDialog dialog(nullptr, heading, initial);
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setFileMode(QFileDialog::AnyFile);

  const auto currentName =
      options.value(QStringLiteral("current_name")).toString();
  if (!currentName.isEmpty())
    dialog.selectFile(currentName);

  const auto accept = options.value(QStringLiteral("accept_label")).toString();
  if (!accept.isEmpty())
    dialog.setLabelText(QFileDialog::Accept, accept);

  if (execStyledDialog(dialog, heading, initial) != QDialog::Accepted) {
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
  const QString heading =
      title.isEmpty() ? LunaDash::translate("Choose folder") : title;

  QFileDialog dialog(nullptr, heading, initial);
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setOption(QFileDialog::ShowDirsOnly, true);

  if (execStyledDialog(dialog, heading, initial) != QDialog::Accepted ||
      dialog.selectedFiles().isEmpty()) {
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
