#include <LuDash/file_manager_actions/FileManagerActions.h>
#include <LuDash/file_association_ui/FileAssociationUi.h>
#include <LuDash/file_associations/FileAssociations.h>
#include <LuDash/file_operations/FileOperations.h>
#include <LuDash/file_icons/FileIcons.h>
#include <LuDash/default_applications/DefaultApplications.h>
#include <LuDash/localization/Localization.h>
#include <LuDash/theme/DesktopTheme.h>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QtWidgets>
#include <algorithm>
#include <functional>

namespace LuDash {
namespace {
QStringList localFiles(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) return {};
    QStringList result;
    for (const auto& url : mime->urls()) {
        if (!url.isLocalFile()) return {};
        result.append(QFileInfo(url.toLocalFile()).absoluteFilePath());
    }
    result.removeDuplicates();
    return result;
}
bool clipboardCut(const QMimeData* mime) {
    return mime && (mime->data("application/x-kde-cutselection") == "1" ||
                    mime->data("x-special/gnome-copied-files").startsWith("cut\n"));
}
void configureDialog(QDialog* dialog) {
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    watchDesktopTheme(dialog);
}
} // namespace
FileManagerActions::FileManagerActions(QWidget* page)
    : QObject(page), page_(page), details_(page->findChild<QTreeView*>("fileView")),
      icons_(page->findChild<QListView*>("fileIcons")),
      location_(page->findChild<QLineEdit*>("fileLocation")),
      search_(page->findChild<QLineEdit*>("fileSearch")),
      status_(page->findChild<QLabel*>("fileStatus")),
      folderTitle_(page->findChild<QLabel*>("fileFolderTitle")),
      places_(page->findChild<QListWidget*>("filePlaces")),
      mode_(page->findChild<QPushButton*>("fileMode")),
      hidden_(page->findChild<QPushButton*>("fileHidden")),
      back_(page->findChild<QPushButton*>("fileBack")),
      forward_(page->findChild<QPushButton*>("fileForward")) {
    auto action = [this](const QString& id, const QString& label, const QKeySequence& key,
                         const std::function<void()>& handler, const char* button = nullptr,
                         bool global = false) {
        auto* item = new QAction(label, this); item->setObjectName("filesAction_" + id);
        item->setShortcut(key); item->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        if (global) page_->addAction(item);
        else { details_->addAction(item); icons_->addAction(item); }
        connect(item, &QAction::triggered, this, handler); actions_.insert(id, item);
        if (button) {
            auto* control = page_->findChild<QPushButton*>(button);
            connect(control, &QPushButton::clicked, item, &QAction::trigger);
            connect(item, &QAction::changed, control, [=] { control->setEnabled(item->isEnabled()); });
        }
    };
    action("open", translate("Open"), QKeySequence(Qt::Key_Return), [this] { openSelection(); });
    action("openWith", translate("Open with…"), {}, [this] { openSelection(true); });
    action("setDefault", translate("Set default application for this file type..."), {}, [this] {
        const auto paths = selection();
        if (paths.size() == 1) chooseFileDefault(page_, paths.first(), [this](const QString& error) {
            status_->setText(error.isEmpty() ? translate("File association saved.") : error);
        });
    });
    action("resetDefault", translate("Reset default application for this file type"), {}, [this] {
        const auto paths = selection(); if (paths.size() != 1) return;
        QString error;
        FileAssociations().removeRule(FileAssociations().keyForFile(paths.first()), &error);
        status_->setText(error.isEmpty() ? translate("File association removed.") : error);
    });
    action("copy", translate("Copy"), QKeySequence::Copy, [this] { copySelection(false); }, "fileCopy");
    action("cut", translate("Cut"), QKeySequence::Cut, [this] { copySelection(true); }, "fileCut");
    action("paste", translate("Paste"), QKeySequence::Paste, [this] { paste(currentPath_); }, "filePaste");
    action("rename", translate("Rename"), QKeySequence(Qt::Key_F2), [this] { renameSelection(); }, "fileRename");
    action("trash", translate("Move to Trash"), QKeySequence::Delete, [this] { trashSelection(); }, "fileTrash");
    action("newFile", translate("New file"), QKeySequence("Ctrl+N"), [this] { createEntry(false, currentPath_); }, "fileNewFile");
    action("newFolder", translate("New folder"), QKeySequence("Ctrl+Shift+N"), [this] { createEntry(true, currentPath_); }, "fileNewFolder");
    action("terminal", translate("Open terminal here"), QKeySequence("Ctrl+Alt+T"), [this] { openTerminal(currentPath_); }, "fileTerminal");
    action("properties", translate("Properties"), QKeySequence("Alt+Return"), [this] { properties(); });
    action("selectAll", translate("Select all"), QKeySequence::SelectAll, [this] { activeView()->selectAll(); });
    action("copyPath", translate("Copy paths"), QKeySequence("Ctrl+Shift+C"), [this] {
        auto paths = selection(); if (paths.isEmpty()) paths << currentPath_;
        QApplication::clipboard()->setText(paths.join('\n'));
    });
    action("refresh", translate("Refresh"), QKeySequence::Refresh, [this] { installModel(); refreshPlaces(); }, nullptr, true);
    actions_.value("refresh")->setShortcuts({QKeySequence("F5"), QKeySequence("Ctrl+R")});
    action("hidden", translate("Show hidden files"), QKeySequence("Ctrl+H"), [this] { hidden_->click(); });
    action("location", translate("Location"), QKeySequence("Ctrl+L"), [this] { location_->setFocus(); location_->selectAll(); }, nullptr, true);
    action("back", translate("Back"), QKeySequence("Alt+Left"), [this] {
        if (historyIndex_ > 0) navigate(history_.at(--historyIndex_), false);
    }, "fileBack", true);
    action("forward", translate("Forward"), QKeySequence("Alt+Right"), [this] {
        if (historyIndex_ + 1 < history_.size()) navigate(history_.at(++historyIndex_), false);
    }, "fileForward", true);
    action("up", translate("Up"), QKeySequence("Alt+Up"), [this] {
        QDir dir(currentPath_); if (dir.cdUp()) navigate(dir.absolutePath());
    }, "fileUp", true);
    connect(page_->findChild<QPushButton*>("fileAssociations"), &QPushButton::clicked, this,
            [this] { showFileAssociationSettings(page_); });
    connect(location_, &QLineEdit::returnPressed, this, [this] { navigate(location_->text()); });
    connect(places_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { navigate(item->data(Qt::UserRole).toString()); });
    places_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(places_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const bool keyboard = position.x() < 0 || position.y() < 0;
        const auto* item = keyboard ? places_->currentItem() : places_->itemAt(position);
        if (!item) return;
        const auto path = item->data(Qt::UserRole).toString();
        auto* menu = new QMenu(page_); menu->setAttribute(Qt::WA_DeleteOnClose);
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        menu->addAction(translate("Open"), this, [this, path] { navigate(path); });
        menu->addAction(translate("Open in a new window"), this, [this, path] {
            if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--app", "files", "--builtin", "--path", path}))
                status_->setText(translate("Could not open a new Files window."));
        });
        menu->addAction(translate("Open terminal here"), this, [this, path] { openTerminal(path); });
        menu->addAction(translate("Copy paths"), this, [path] { QApplication::clipboard()->setText(path); });
        menu->popup(places_->viewport()->mapToGlobal(keyboard ? places_->visualItemRect(item).center() : position));
    });
    auto* stack = page_->findChild<QStackedWidget*>("fileViewStack");
    connect(mode_, &QPushButton::toggled, this, [this, stack](bool checked) {
        stack->setCurrentIndex(checked ? 1 : 0);
        mode_->setIcon(fileIcon(checked ? FileIcon::Grid : FileIcon::List));
        QSettings().setValue("files/iconView", checked);
        updateActions();
    });
    connect(hidden_, &QPushButton::toggled, this, [this](bool checked) {
        if (model_) model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System |
                                     (checked ? QDir::Hidden : QDir::Filters{}));
        QSettings().setValue("files/showHidden", checked);
    });
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        model_->setNameFilters(text.isEmpty() ? QStringList{} : QStringList{"*" + text + "*"});
    });
    connect(details_->header(), &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) {
        sortColumn_ = column; ascending_ = order == Qt::AscendingOrder;
        QSettings settings; settings.setValue("files/sortColumn", column); settings.setValue("files/sortAscending", ascending_);
    });
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, [this] { updateActions(); });
    QSettings settings;
    hidden_->setChecked(settings.value("files/showHidden", false).toBool());
    mode_->setChecked(settings.value("files/iconView", true).toBool());
    sortColumn_ = qBound(0, settings.value("files/sortColumn", 0).toInt(), 3);
    ascending_ = settings.value("files/sortAscending", true).toBool();
    installModel();
    for (auto* view : {static_cast<QAbstractItemView*>(details_), static_cast<QAbstractItemView*>(icons_)}) {
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        view->setDragEnabled(true); view->setAcceptDrops(true); view->viewport()->setAcceptDrops(true);
        view->setDragDropMode(QAbstractItemView::DragDrop); view->setDefaultDropAction(Qt::CopyAction);
        view->viewport()->installEventFilter(this);
        connect(view, &QAbstractItemView::customContextMenuRequested, this,
                [this, view](const QPoint& position) { contextMenu(view, position); });
        connect(view, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& index) {
            if (!index.isValid()) return;
            const auto info = model_->fileInfo(index);
            if (info.isDir()) navigate(info.absoluteFilePath());
            else openAssociatedFiles(page_, {info.absoluteFilePath()}, false, [this](const QString& message) { status_->setText(message); });
        });
    }
    refreshPlaces();
    navigate(QDir::homePath());
    QTimer::singleShot(0, this, [this] {
        QString message;
        migrateLegacyFileAssociations(&message);
        if (!message.isEmpty()) status_->setText(message);
        showFileManagerFirstRun(page_);
    });
}
QAbstractItemView* FileManagerActions::activeView() const {
    return mode_->isChecked() ? static_cast<QAbstractItemView*>(icons_) : static_cast<QAbstractItemView*>(details_);
}
QStringList FileManagerActions::selection() const {
    QStringList paths;
    if (!activeView()->selectionModel()) return paths;
    for (const auto& index : activeView()->selectionModel()->selectedIndexes())
        if (index.column() == 0) paths << model_->filePath(index);
    paths.removeDuplicates();
    return paths;
}
void FileManagerActions::installModel() {
    auto* oldModel = model_;
    const auto oldSelections = page_->findChildren<QItemSelectionModel*>();
    model_ = new QFileSystemModel(this);
    model_->setReadOnly(true); model_->setNameFilterDisables(false);
    model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System |
                      (hidden_->isChecked() ? QDir::Hidden : QDir::Filters{}));
    const auto path = currentPath_.isEmpty() ? QDir::homePath() : currentPath_;
    model_->setRootPath(path);
    details_->setModel(model_); icons_->setModel(model_);
    auto* unusedSelection = icons_->selectionModel();
    icons_->setSelectionModel(details_->selectionModel());
    unusedSelection->deleteLater();
    for (auto* selection : oldSelections) if (selection->model() == oldModel) selection->deleteLater();
    if (oldModel) oldModel->deleteLater();
    connect(details_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });
    connect(model_, &QFileSystemModel::directoryLoaded, this, [this](const QString& loaded) {
        if (loaded == currentPath_ && !busy_)
            status_->setText(translate("%1 items").arg(model_->rowCount(details_->rootIndex())));
    });
    details_->setRootIndex(model_->index(path)); icons_->setRootIndex(model_->index(path));
    details_->setColumnWidth(0, 290);
    model_->setNameFilters(search_->text().isEmpty() ? QStringList{} : QStringList{"*" + search_->text() + "*"});
    sort(sortColumn_, ascending_); updateActions();
}
void FileManagerActions::navigate(const QString& path, bool remember) {
    const QFileInfo info(path);
    if (!info.isDir()) {
        location_->setText(currentPath_);
        status_->setText(translate("Folder not found: ") + path);
        return;
    }
    currentPath_ = info.absoluteFilePath();
    if (remember && (historyIndex_ < 0 || history_.value(historyIndex_) != currentPath_)) {
        history_ = history_.mid(0, historyIndex_ + 1); history_ << currentPath_; ++historyIndex_;
    }
    location_->setText(currentPath_);
    model_->setRootPath(currentPath_);
    details_->setRootIndex(model_->index(currentPath_)); icons_->setRootIndex(model_->index(currentPath_));
    details_->selectionModel()->clear();
    folderTitle_->setText(currentPath_ == QDir::homePath() ? translate("Home") :
                         info.fileName().isEmpty() ? translate("File system") : info.fileName());
    for (int row = 0; row < places_->count(); ++row)
        places_->item(row)->setSelected(places_->item(row)->data(Qt::UserRole).toString() == currentPath_);
    if (!busy_) status_->setText(translate("%1 items").arg(model_->rowCount(details_->rootIndex())));
    updateActions();
}
void FileManagerActions::refreshPlaces() {
    places_->clear();
    auto add = [this](const QString& label, const QString& path, FileIcon icon) {
        if (path.isEmpty()) return;
        auto* item = new QListWidgetItem(fileIcon(icon), label, places_); item->setData(Qt::UserRole, path);
    };
    add(translate("Home"), QDir::homePath(), FileIcon::Home);
    add(translate("Desktop"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), FileIcon::Desktop);
    add(translate("Documents"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), FileIcon::File);
    add(translate("Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), FileIcon::Download);
    add(translate("Pictures"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), FileIcon::Picture);
    add(translate("Music"), QStandardPaths::writableLocation(QStandardPaths::MusicLocation), FileIcon::Music);
    add(translate("Videos"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), FileIcon::Video);
    add(translate("File system"), "/", FileIcon::Drive);
    for (const auto& volume : QStorageInfo::mountedVolumes())
        if (volume.isValid() && volume.isReady() && (volume.rootPath().startsWith("/run/media/") ||
            volume.rootPath().startsWith("/media/") || volume.rootPath().startsWith("/mnt/")))
            add(volume.displayName(), volume.rootPath(), FileIcon::Drive);
}
void FileManagerActions::updateActions() {
    const auto paths = selection();
    for (const auto& id : {"open", "copy", "cut", "trash"}) actions_.value(id)->setEnabled(!busy_ && !paths.isEmpty());
    const bool files = !paths.isEmpty() && std::all_of(paths.begin(), paths.end(), [](const auto& path) { return QFileInfo(path).isFile(); });
    actions_.value("openWith")->setEnabled(!busy_ && files);
    actions_.value("setDefault")->setEnabled(!busy_ && files && paths.size() == 1);
    actions_.value("resetDefault")->setEnabled(!busy_ && files && paths.size() == 1 && !FileAssociations().applicationForFile(paths.first()).isEmpty());
    actions_.value("rename")->setEnabled(!busy_ && paths.size() == 1);
    actions_.value("paste")->setEnabled(!busy_ && !clipboardFiles().isEmpty() && QFileInfo(currentPath_).isWritable());
    for (const auto& id : {"newFile", "newFolder"}) actions_.value(id)->setEnabled(!busy_ && QFileInfo(currentPath_).isWritable());
    actions_.value("back")->setEnabled(historyIndex_ > 0);
    actions_.value("forward")->setEnabled(historyIndex_ + 1 < history_.size());
}
void FileManagerActions::openSelection(bool chooseApplication) {
    const auto paths = selection(); if (paths.isEmpty() || busy_) return;
    if (paths.size() == 1 && QFileInfo(paths.first()).isDir()) { navigate(paths.first()); return; }
    openAssociatedFiles(page_, paths, chooseApplication, [this](const QString& message) { status_->setText(message); });
}
void FileManagerActions::contextMenu(QAbstractItemView* view, const QPoint& position) {
    QModelIndex index;
    const bool keyboard = position.x() < 0 || position.y() < 0;
    if (keyboard) index = view->currentIndex(); else index = view->indexAt(position);
    if (index.isValid() && !view->selectionModel()->isSelected(index.siblingAtColumn(0)))
        view->selectionModel()->setCurrentIndex(index.siblingAtColumn(0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    else if (!index.isValid() && !keyboard) view->selectionModel()->clear();
    updateActions();
    const auto paths = selection();
    const bool directory = paths.size() == 1 && QFileInfo(paths.first()).isDir();
    const QString destination = directory ? paths.first() : currentPath_;
    auto* menu = new QMenu(page_); menu->setObjectName("fileContextMenu"); menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    if (!paths.isEmpty()) {
        menu->addAction(actions_.value("open"));
        if (!directory) {
            menu->addAction(actions_.value("openWith"));
            menu->addAction(actions_.value("setDefault"));
            menu->addAction(actions_.value("resetDefault"));
        }
        if (directory) menu->addAction(translate("Open in a new window"), this, [this, destination] {
            if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--app", "files", "--builtin", "--path", destination}))
                status_->setText(translate("Could not open a new Files window."));
        });
        menu->addSeparator(); menu->addAction(actions_.value("cut")); menu->addAction(actions_.value("copy"));
        menu->addAction(actions_.value("rename")); menu->addAction(actions_.value("trash")); menu->addSeparator();
    }
    auto* pasteAction = menu->addAction(directory ? translate("Paste into this folder") : translate("Paste"), this,
                                      [this, destination] { paste(destination); });
    pasteAction->setEnabled(!busy_ && !clipboardFiles().isEmpty() && QFileInfo(destination).isWritable());
    if (paths.isEmpty() || directory) {
        auto* newFile = menu->addAction(translate("New file"), this, [this, destination] { createEntry(false, destination); });
        auto* newFolder = menu->addAction(translate("New folder"), this, [this, destination] { createEntry(true, destination); });
        newFile->setEnabled(!busy_ && QFileInfo(destination).isWritable()); newFolder->setEnabled(newFile->isEnabled());
        menu->addAction(translate("Open terminal here"), this, [this, destination] { openTerminal(destination); });
    }
    menu->addAction(actions_.value("copyPath")); menu->addAction(actions_.value("properties"));
    menu->addSeparator();
    auto* sorting = menu->addMenu(translate("Sort by"));
    const QStringList labels{translate("Name"), translate("Size"), translate("Type"), translate("Modified")};
    for (int column = 0; column < labels.size(); ++column) {
        auto* item = sorting->addAction(labels.at(column), this, [this, column] { sort(column, ascending_); });
        item->setCheckable(true); item->setChecked(sortColumn_ == column);
    }
    sorting->addSeparator();
    auto* descending = sorting->addAction(translate("Descending order"), this, [this] { sort(sortColumn_, !ascending_); });
    descending->setCheckable(true); descending->setChecked(!ascending_);
    auto* hidden = menu->addAction(translate("Show hidden files"), hidden_, &QPushButton::click);
    hidden->setCheckable(true); hidden->setChecked(hidden_->isChecked());
    menu->addAction(actions_.value("refresh"));
    menu->addAction(translate("File associations"), this, [this] { showFileAssociationSettings(page_); });
    const auto point = keyboard ? (index.isValid() ? view->visualRect(index).center() : QPoint(16, 16)) : position;
    menu->popup(view->viewport()->mapToGlobal(point));
}
void FileManagerActions::copySelection(bool cut) {
    const auto paths = selection(); if (paths.isEmpty() || busy_) return;
    auto* mime = new QMimeData;
    QList<QUrl> urls; QByteArray gnome = cut ? "cut\n" : "copy\n";
    for (const auto& path : paths) { const auto url = QUrl::fromLocalFile(path); urls << url; gnome += url.toEncoded() + '\n'; }
    mime->setUrls(urls); mime->setData("application/x-kde-cutselection", cut ? "1" : "0");
    mime->setData("x-special/gnome-copied-files", gnome); QApplication::clipboard()->setMimeData(mime);
    status_->setText(cut ? translate("Selection cut. Choose a folder and Paste.") : translate("Selection copied. Choose a folder and Paste."));
}
QStringList FileManagerActions::clipboardFiles() const { return localFiles(QApplication::clipboard()->mimeData()); }
void FileManagerActions::paste(const QString& destination) {
    const auto paths = clipboardFiles();
    const bool cut = clipboardCut(QApplication::clipboard()->mimeData());
    if (!paths.isEmpty()) run(cut ? "move" : "copy", paths, destination, cut);
}
void FileManagerActions::run(const QString& operation, const QStringList& paths, const QString& destination, bool clearClipboard) {
    if (busy_) return;
    busy_ = true; updateActions(); status_->setText(translate("Working..."));
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [=, this] {
        const auto error = watcher->result(); busy_ = false;
        if (error.isEmpty() && clearClipboard && clipboardFiles() == paths && clipboardCut(QApplication::clipboard()->mimeData()))
            QApplication::clipboard()->clear();
        status_->setText(error.isEmpty() ? translate("Operation completed.") : error);
        updateActions(); watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([operation, paths, destination] { return performFileOperation(operation, paths, destination); }));
}
void FileManagerActions::createEntry(bool folder, const QString& destination) {
    if (busy_) return;
    auto* dialog = new QInputDialog(page_); configureDialog(dialog);
    dialog->setWindowTitle(folder ? translate("New folder") : translate("New file"));
    dialog->setLabelText(folder ? translate("Folder name") : translate("File name"));
    connect(dialog, &QInputDialog::textValueSelected, this, [this, destination, folder](const QString& name) {
        if (busy_) return;
        bool success = false;
        if (validFileName(name)) {
            if (folder) success = QDir(destination).mkdir(name);
            else { QFile file(QDir(destination).filePath(name)); success = file.open(QIODevice::WriteOnly | QIODevice::NewOnly); }
        }
        status_->setText(success ? translate("Operation completed.") : translate("Could not create the item. Check its name and permissions; existing files are never overwritten."));
    });
    dialog->open();
}
void FileManagerActions::renameSelection() {
    const auto paths = selection(); if (busy_ || paths.size() != 1) return;
    const QFileInfo info(paths.first());
    auto* dialog = new QInputDialog(page_); configureDialog(dialog);
    dialog->setWindowTitle(translate("Rename")); dialog->setLabelText(translate("New name")); dialog->setTextValue(info.fileName());
    connect(dialog, &QInputDialog::textValueSelected, this, [this, info](const QString& name) {
        if (busy_ || name == info.fileName()) return;
        if (!validFileName(name) || QFileInfo::exists(info.dir().filePath(name)) ||
            QFileInfo(info.dir().filePath(name)).isSymLink() || !QDir().rename(info.absoluteFilePath(), info.dir().filePath(name)))
            status_->setText(translate("Could not rename item. Nothing was overwritten."));
    });
    dialog->open();
}
void FileManagerActions::trashSelection() {
    const auto paths = selection(); if (busy_ || paths.isEmpty()) return;
    auto* confirm = new QMessageBox(QMessageBox::Question, translate("Move to Trash"),
        translate("Move %1 selected items to Trash?").arg(paths.size()), QMessageBox::Yes | QMessageBox::Cancel, page_);
    configureDialog(confirm); confirm->setTextFormat(Qt::PlainText); confirm->setDefaultButton(QMessageBox::Cancel);
    connect(confirm, &QMessageBox::finished, this, [this, paths](int result) {
        if (result == QMessageBox::Yes) run("trash", paths, {});
    });
    confirm->open();
}
void FileManagerActions::openTerminal(const QString& directory) {
    QString error; auto command = defaultApplicationCommand("terminal", &error);
    if (!error.isEmpty() || command.isEmpty()) {
        status_->setText(error.isEmpty() ? translate("No default terminal is configured.") : error); return;
    }
    QProcess process; process.setProgram(command.takeFirst()); process.setArguments(command);
    process.setWorkingDirectory(directory);
    auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("PWD", directory);
    process.setProcessEnvironment(environment);
    if (!process.startDetached()) status_->setText(translate("Could not start the terminal: %1").arg(process.errorString()));
}
void FileManagerActions::properties() {
    auto paths = selection(); if (paths.isEmpty()) paths << currentPath_;
    auto* dialog = new QDialog(page_); configureDialog(dialog); dialog->setWindowTitle(translate("Properties")); dialog->resize(620, 400);
    auto* layout = new QVBoxLayout(dialog); auto* text = new QPlainTextEdit(dialog); text->setReadOnly(true);
    QStringList sections;
    for (const auto& path : paths) {
        const QFileInfo info(path);
        QStringList lines{translate("Name") + ": " + info.fileName(), translate("Location") + ": " + path,
            translate("MIME type") + ": " + FileAssociations::mimeTypeForFile(path),
            translate("Size") + ": " + (info.isDir() ? translate("Folder contents are not included in this size.") : QLocale().formattedDataSize(info.size())),
            translate("Modified") + ": " + QLocale().toString(info.lastModified(), QLocale::ShortFormat),
            translate("Owner") + ": " + info.owner(), translate("Group") + ": " + info.group()};
        if (info.isSymLink()) lines << translate("Symbolic link target") + ": " + info.symLinkTarget();
        sections << lines.join('\n');
    }
    text->setPlainText(sections.join("\n\n")); layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject); dialog->open();
}
void FileManagerActions::sort(int column, bool ascending) {
    sortColumn_ = qBound(0, column, 3); ascending_ = ascending;
    details_->sortByColumn(sortColumn_, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
    model_->sort(sortColumn_, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}
bool FileManagerActions::eventFilter(QObject* watched, QEvent* event) {
    auto* view = watched == details_->viewport() ? static_cast<QAbstractItemView*>(details_) :
                 watched == icons_->viewport() ? static_cast<QAbstractItemView*>(icons_) : nullptr;
    if (!view) return QObject::eventFilter(watched, event);
    if (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove && event->type() != QEvent::Drop)
        return QObject::eventFilter(watched, event);
    auto* drop = static_cast<QDropEvent*>(event);
    const auto paths = localFiles(drop->mimeData());
    const auto index = view->indexAt(drop->position().toPoint());
    const auto destination = index.isValid() && model_->isDir(index) ? model_->filePath(index) : currentPath_;
    if (busy_ || paths.isEmpty() || !QFileInfo(destination).isWritable()) { drop->ignore(); return true; }
    const bool move = drop->modifiers().testFlag(Qt::ShiftModifier);
    const auto action = move ? Qt::MoveAction : Qt::CopyAction;
    if (!drop->possibleActions().testFlag(action)) { drop->ignore(); return true; }
    drop->setDropAction(action); drop->accept();
    if (event->type() == QEvent::Drop) run(move ? "move" : "copy", paths, destination);
    return true;
}
} // namespace LuDash
