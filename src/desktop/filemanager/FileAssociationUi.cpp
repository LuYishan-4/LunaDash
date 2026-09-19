#include "desktop/filemanager/FileAssociationUi.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/filemanager/FileAssociations.hpp"
#include "desktop/theme/DesktopTheme.hpp"
#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QtConcurrent/QtConcurrentRun>
#include <QtWidgets>

namespace LunaDash {
namespace {
using Report = std::function<void(const QString &)>;
QLabel *explanation(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  return label;
}
QDialog *dialogFor(QWidget *parent, const QString &title, const QString &name) {
  auto *dialog = new QDialog(parent);
  dialog->setObjectName(name);
  dialog->setWindowTitle(title);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowModality(Qt::WindowModal);
  dialog->resize(580, 480);
  watchDesktopTheme(dialog);
  return dialog;
}
void launch(QWidget *parent, const QString &app, const QStringList &files,
            const Report &report) {
  auto *watcher = new QFutureWatcher<QString>(parent);
  QObject::connect(watcher, &QFutureWatcher<QString>::finished, parent,
                   [watcher, report] {
                     report(watcher->result());
                     watcher->deleteLater();
                   });
  watcher->setFuture(QtConcurrent::run([app, files] {
    QString error;
    launchFilesWithApplication(app, files, &error);
    return error;
  }));
}
void choose(QWidget *parent, const QString &key, const QString &mime,
            const QStringList &files, const Report &report) {
  auto *dialog = dialogFor(parent, translate("Choose an application"),
                           "fileApplicationChooser");
  auto *layout = new QVBoxLayout(dialog);
  const auto typeLabel = key.startsWith("ext:") ? "*." + key.mid(4) : mime;
  layout->addWidget(explanation(
      translate("Choose an application for %1").arg(typeLabel), dialog));
  if (!files.isEmpty())
    layout->addWidget(explanation(QFileInfo(files.first()).fileName(), dialog));
  auto *search = new QLineEdit(dialog);
  search->setObjectName("fileApplicationSearch");
  search->setPlaceholderText(translate("Search installed applications"));
  layout->addWidget(search);
  auto *showAll =
      new QCheckBox(translate("Show all installed applications"), dialog);
  layout->addWidget(showAll);
  auto *apps = new QListWidget(dialog);
  apps->setObjectName("fileApplicationList");
  layout->addWidget(apps, 1);
  auto *remember = new QCheckBox(
      translate("Always use this application for %1 in Files").arg(typeLabel),
      dialog);
  remember->setObjectName("rememberFileApplication");
  remember->setChecked(true);
  remember->setVisible(!files.isEmpty());
  layout->addWidget(remember);
  auto *system = new QCheckBox(
      translate("Also make it the system default for %1").arg(mime), dialog);
  system->setObjectName("systemFileApplication");
  layout->addWidget(system);
  layout->addWidget(explanation(
      translate("System defaults apply to MIME types and may affect other "
                "extensions. Extension-only rules affect LunaDash Files."),
      dialog));
  auto *errorLabel = explanation({}, dialog);
  errorLabel->setObjectName("fileAssociationError");
  layout->addWidget(errorLabel);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, dialog);
  auto *accept = buttons->addButton(
      files.isEmpty() ? translate("Save file association") : translate("Open"),
      QDialogButtonBox::AcceptRole);
  accept->setObjectName("confirmFileApplication");
  layout->addWidget(buttons);
  auto entries = fileApplications(mime);
  if (!systemFileApplication(mime).isEmpty())
    entries.prepend({"__system__", translate("Use the current system default"),
                     "application-default-icon", true, false});
  const auto previous = FileAssociations()
                            .read()
                            .value("associations")
                            .toObject()
                            .value(key)
                            .toObject()
                            .value("desktopId")
                            .toString();
  for (const auto &entry : entries) {
    const auto icon = QFileInfo(entry.icon).isAbsolute()
                          ? QIcon(entry.icon)
                          : QIcon::fromTheme(entry.icon);
    auto *item = new QListWidgetItem(
        icon,
        entry.name +
            (entry.systemDefault ? " · " + translate("System default") : ""),
        apps);
    item->setData(Qt::UserRole, entry.id);
    item->setData(Qt::UserRole + 1, entry.recommended || entry.systemDefault ||
                                        entry.id == previous);
    item->setToolTip(entry.id == "__system__" ? systemFileApplication(mime)
                                              : entry.id);
    if (entry.id == previous)
      apps->setCurrentItem(item);
  }
  bool hasRecommended = false;
  for (int row = 0; row < apps->count(); ++row)
    hasRecommended |= apps->item(row)->data(Qt::UserRole + 1).toBool();
  showAll->setChecked(!hasRecommended);
  auto updateButtons = [=] {
    const auto *item = apps->currentItem();
    accept->setEnabled(item && !item->isHidden());
    system->setEnabled(item &&
                       item->data(Qt::UserRole).toString() != "__system__" &&
                       remember->isChecked());
    if (!system->isEnabled())
      system->setChecked(false);
  };
  auto filter = [=] {
    QListWidgetItem *first = nullptr;
    for (int row = 0; row < apps->count(); ++row) {
      auto *item = apps->item(row);
      const bool visible =
          (showAll->isChecked() || item->data(Qt::UserRole + 1).toBool()) &&
          (item->text().contains(search->text(), Qt::CaseInsensitive) ||
           item->data(Qt::UserRole)
               .toString()
               .contains(search->text(), Qt::CaseInsensitive));
      item->setHidden(!visible);
      if (visible && !first)
        first = item;
    }
    if (!apps->currentItem() || apps->currentItem()->isHidden())
      apps->setCurrentItem(first);
    updateButtons();
  };
  QObject::connect(search, &QLineEdit::textChanged, dialog, filter);
  QObject::connect(showAll, &QCheckBox::toggled, dialog, filter);
  QObject::connect(apps, &QListWidget::currentRowChanged, dialog,
                   updateButtons);
  QObject::connect(remember, &QCheckBox::toggled, dialog, updateButtons);
  QObject::connect(buttons, &QDialogButtonBox::rejected, dialog,
                   &QDialog::reject);
  QObject::connect(accept, &QPushButton::clicked, dialog, [=] {
    const auto *item = apps->currentItem();
    if (!item || item->isHidden())
      return;
    const auto id = item->data(Qt::UserRole).toString();
    QString error;
    if (remember->isChecked() &&
        !FileAssociations().setRule(key, id, mime, &error)) {
      errorLabel->setText(error);
      return;
    }
    if (system->isChecked() && !setSystemFileApplication(id, mime, &error)) {
      errorLabel->setText(translate("The Files rule was saved, but the system "
                                    "default was not changed. %1")
                              .arg(error));
      return;
    }
    if (!files.isEmpty())
      launch(parent, id, files, report);
    else
      report({});
    dialog->accept();
  });
  filter();
  if (entries.isEmpty())
    errorLabel->setText(translate("No installed applications were found. "
                                  "Install an application, then try again."));
  dialog->open();
  search->setFocus();
}
} // namespace

void openAssociatedFiles(QWidget *parent, const QStringList &files,
                         bool chooseApplication,
                         const std::function<void(const QString &)> &report) {
  if (files.isEmpty())
    return;
  if (files.size() > 256) {
    report(translate("Select between 1 and 256 items."));
    return;
  }
  const FileAssociations store;
  QString error;
  const auto config = store.read(&error);
  if (!error.isEmpty())
    report(error);
  const auto key = store.keyForFile(files.first());
  for (const auto &path : files) {
    if (!QFileInfo(path).isFile()) {
      report(translate("Source no longer exists: %1").arg(path));
      return;
    }
    if (store.keyForFile(path) != key) {
      report(translate(
          "Select files of one type to choose their application together."));
      return;
    }
  }
  const auto mime = FileAssociations::mimeTypeForFile(files.first());
  const auto configured = config.value("associations")
                              .toObject()
                              .value(key)
                              .toObject()
                              .value("desktopId")
                              .toString();
  QString application =
      configured == "__system__" ? systemFileApplication(mime) : configured;
  if (!configured.isEmpty() &&
      (application.isEmpty() || !fileApplicationAvailable(application))) {
    report(translate(
        "The saved application is unavailable. Choose another application."));
    chooseApplication = true;
  }
  if (application.isEmpty() && !config.value("askOnFirstOpen").toBool(true) &&
      !config.isEmpty())
    application = systemFileApplication(mime);
  // A file is always data supplied to an installed handler. Never execute a
  // selected .desktop entry or binary through QDesktopServices::openUrl.
  if (chooseApplication || application.isEmpty() || config.isEmpty())
    choose(parent, key, mime, files, report);
  else
    launch(parent, application, files, report);
}
void chooseFileDefault(QWidget *parent, const QString &file,
                       const std::function<void(const QString &)> &report) {
  if (!QFileInfo(file).isFile()) {
    report(translate("Source no longer exists: %1").arg(file));
    return;
  }
  choose(parent, FileAssociations().keyForFile(file),
         FileAssociations::mimeTypeForFile(file), {}, report);
}
void showFileAssociationSettings(QWidget *parent) {
  if (auto *existing =
          parent->findChild<QDialog *>("fileAssociationSettings")) {
    existing->raise();
    existing->activateWindow();
    return;
  }
  auto *dialog = dialogFor(parent, translate("File associations"),
                           "fileAssociationSettings");
  auto *layout = new QVBoxLayout(dialog);
  auto *ask =
      new QCheckBox(translate("Ask before opening each new file type"), dialog);
  ask->setObjectName("askOnFirstFileOpen");
  layout->addWidget(ask);
  auto *list = new QTreeWidget(dialog);
  list->setObjectName("fileAssociationRules");
  list->setHeaderLabels({translate("File type"), translate("Application"),
                         translate("MIME type")});
  list->setRootIsDecorated(false);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(list, 1);
  auto *row = new QHBoxLayout;
  auto *extension = new QLineEdit(dialog);
  extension->setObjectName("fileAssociationExtension");
  extension->setPlaceholderText(
      translate("Extension, for example txt or tar.gz"));
  auto *add = new QPushButton(translate("Add file association"), dialog);
  row->addWidget(extension, 1);
  row->addWidget(add);
  layout->addLayout(row);
  auto *actions = new QHBoxLayout;
  auto *edit = new QPushButton(translate("Change application"), dialog);
  auto *remove = new QPushButton(translate("Remove file association"), dialog);
  auto *refresh = new QPushButton(translate("Refresh"), dialog);
  actions->addWidget(edit);
  actions->addWidget(remove);
  actions->addWidget(refresh);
  layout->addLayout(actions);
  auto *status = explanation({}, dialog);
  layout->addWidget(status);
  layout->addWidget(explanation(
      translate(
          "Removing a rule restores first-use prompting or the system default. "
          "It does not delete files or reset other applications."),
      dialog));
  auto *close = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
  layout->addWidget(close);
  QObject::connect(close, &QDialogButtonBox::rejected, dialog,
                   &QDialog::reject);
  auto reload = [=] {
    QString error;
    const auto config = FileAssociations().read(&error);
    status->setText(error);
    {
      const QSignalBlocker blocker(ask);
      ask->setChecked(config.value("askOnFirstOpen").toBool(true));
    }
    list->clear();
    const auto rules = config.value("associations").toObject();
    for (auto it = rules.begin(); it != rules.end(); ++it) {
      const auto rule = it.value().toObject();
      const auto id = rule.value("desktopId").toString();
      const auto label =
          id == "__system__" ? translate("Use the current system default") : id;
      auto *item = new QTreeWidgetItem(
          list, {it.key().startsWith("ext:") ? "*." + it.key().mid(4)
                                             : it.key().mid(5),
                 label + (fileApplicationAvailable(id)
                              ? ""
                              : " · " + translate("Unavailable")),
                 rule.value("mimeType").toString()});
      item->setData(0, Qt::UserRole, it.key());
    }
    for (int column = 0; column < 3; ++column)
      list->resizeColumnToContents(column);
    edit->setEnabled(false);
    remove->setEnabled(false);
  };
  QObject::connect(list, &QTreeWidget::itemSelectionChanged, dialog, [=] {
    edit->setEnabled(!list->selectedItems().isEmpty());
    remove->setEnabled(edit->isEnabled());
  });
  QObject::connect(ask, &QCheckBox::toggled, dialog, [=](bool value) {
    QString error;
    if (!FileAssociations().setPreferences(true, value, &error)) {
      reload();
      status->setText(error);
    }
  });
  QObject::connect(refresh, &QPushButton::clicked, dialog, reload);
  QObject::connect(add, &QPushButton::clicked, dialog, [=] {
    const auto key = FileAssociations::keyForExtension(extension->text());
    if (key.isEmpty()) {
      status->setText(
          translate("Enter a valid file extension without a path."));
      return;
    }
    const auto mime = QMimeDatabase()
                          .mimeTypeForFile("example." + key.mid(4),
                                           QMimeDatabase::MatchExtension)
                          .name();
    choose(dialog, key, mime, {}, [reload](const QString &) { reload(); });
  });
  QObject::connect(edit, &QPushButton::clicked, dialog, [=] {
    if (list->selectedItems().isEmpty())
      return;
    const auto *item = list->selectedItems().first();
    choose(dialog, item->data(0, Qt::UserRole).toString(), item->text(2), {},
           [reload](const QString &) { reload(); });
  });
  QObject::connect(remove, &QPushButton::clicked, dialog, [=] {
    if (list->selectedItems().isEmpty())
      return;
    QString error;
    if (!FileAssociations().removeRule(
            list->selectedItems().first()->data(0, Qt::UserRole).toString(),
            &error))
      status->setText(error);
    else
      reload();
  });
  reload();
  dialog->open();
}
void showFileManagerFirstRun(QWidget *parent) {
  QString error;
  const auto config = FileAssociations().read(&error);
  if (!error.isEmpty() || config.value("initialized").toBool())
    return;
  auto *dialog =
      dialogFor(parent, translate("Welcome to Files"), "fileManagerFirstRun");
  dialog->resize(520, 260);
  auto *layout = new QVBoxLayout(dialog);
  layout->addWidget(explanation(
      translate(
          "Choose how Files opens documents. You can change these options "
          "later from File associations or the right-click menu."),
      dialog));
  auto *ask =
      new QCheckBox(translate("Ask before opening each new file type"), dialog);
  ask->setChecked(true);
  layout->addWidget(ask);
  layout->addWidget(
      explanation(translate("When this is disabled, Files uses existing system "
                            "defaults for types without a Files rule."),
                  dialog));
  auto *status = explanation({}, dialog);
  layout->addWidget(status);
  auto *buttons = new QDialogButtonBox(dialog);
  auto *review = buttons->addButton(translate("Review file associations"),
                                    QDialogButtonBox::ActionRole);
  auto *start = buttons->addButton(translate("Start using Files"),
                                   QDialogButtonBox::AcceptRole);
  layout->addWidget(buttons);
  auto finish = [=](bool settings) {
    QString message;
    if (!FileAssociations().setPreferences(true, ask->isChecked(), &message)) {
      status->setText(message);
      return;
    }
    dialog->accept();
    if (settings)
      showFileAssociationSettings(parent);
  };
  QObject::connect(start, &QPushButton::clicked, dialog,
                   [finish] { finish(false); });
  QObject::connect(review, &QPushButton::clicked, dialog,
                   [finish] { finish(true); });
  dialog->open();
}
} // namespace LunaDash
