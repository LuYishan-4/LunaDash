#include "desktop/FileManagerActions/FileManagerActions.hpp"
#include "desktop/FileAssociationUi/FileAssociationUi.hpp"
#include "desktop/FileAssociations/FileAssociations.hpp"
#include "desktop/FileOperations/FileOperations.hpp"
#include "desktop/FileIcons/FileIcons.hpp"
#include "desktop/DefaultApplications/DefaultApplications.hpp"
#include "config/Localization/Localization.hpp"
#include "desktop/DesktopTheme/DesktopTheme.hpp"
#include <QtConcurrent/QtConcurrentRun>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QImageReader>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
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

QString previewCachePath(const QFileInfo& info) {
    const QByteArray seed = info.absoluteFilePath().toUtf8() + '|' +
        QByteArray::number(info.lastModified().toMSecsSinceEpoch()) + '|' +
        QByteArray::number(info.size());
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
        QStringLiteral("/LunaDash/file-previews");
    QDir().mkpath(cacheRoot);
    return cacheRoot + '/' + QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha256).toHex()) + ".jpg";
}

QString prettyDate(const QDateTime& value) {
    return value.isValid() ? QLocale().toString(value, QLocale::LongFormat) : QStringLiteral("—");
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
      forward_(page->findChild<QPushButton*>("fileForward")),
      previewImage_(page->findChild<QLabel*>("filePreviewImage")),
      previewTitle_(page->findChild<QLabel*>("filePreviewTitle")),
      previewMeta_(page->findChild<QLabel*>("filePreviewMeta")),
      previewHint_(page->findChild<QLabel*>("filePreviewHint")) {
    auto action = [this](const QString& id, const QString& label, const QKeySequence& key,
                         const std::function<void()>& handler, const char* button = nullptr,
                         bool global = false) {
        auto* item = new QAction(label, this);
        item->setObjectName("filesAction_" + id);
        item->setShortcut(key);
        item->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        if (global) page_->addAction(item);
        else { details_->addAction(item); icons_->addAction(item); }
        connect(item, &QAction::triggered, this, handler);
        actions_.insert(id, item);
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
        const auto paths = selection();
        if (paths.size() != 1) return;
        QString error;
        FileAssociations().removeRule(FileAssociations().keyForFile(paths.first()), &error);
        status_->setText(error.isEmpty() ? translate("File association removed.") : error);
    });
    action("copy", translate("Copy"), QKeySequence::Copy, [this] { copySelection(false); }, "fileCopy");
    action("cut", translate("Cut"), QKeySequence::Cut, [this] { copySelection(true); }, "fileCut");
    action("paste", translate("Paste"), QKeySequence::Paste, [this] { paste(currentPath_); }, "filePaste");
    action("duplicate", translate("Duplicate"), QKeySequence("Ctrl+D"), [this] { duplicateSelection(); });
    action("copyTo", translate("Copy to…"), {}, [this] { copyOrMoveSelection(false); });
    action("moveTo", translate("Move to…"), {}, [this] { copyOrMoveSelection(true); });
    action("rename", translate("Rename"), QKeySequence(Qt::Key_F2), [this] { renameSelection(); }, "fileRename");
    action("trash", translate("Move to Trash"), QKeySequence::Delete, [this] { trashSelection(); }, "fileTrash");
    action("delete", translate("Delete permanently"), QKeySequence("Shift+Delete"), [this] { deleteSelectionPermanently(); });
    action("newFile", translate("New file"), QKeySequence("Ctrl+N"), [this] { createEntry(false, currentPath_); }, "fileNewFile");
    action("newFolder", translate("New folder"), QKeySequence("Ctrl+Shift+N"), [this] { createEntry(true, currentPath_); }, "fileNewFolder");
    action("terminal", translate("Open terminal here"), QKeySequence("Ctrl+Alt+T"), [this] { openTerminal(currentPath_); }, "fileTerminal");
    action("properties", translate("Properties"), QKeySequence("Alt+Return"), [this] { properties(); });
    action("selectAll", translate("Select all"), QKeySequence::SelectAll, [this] { activeView()->selectAll(); });
    action("copyPath", translate("Copy paths"), QKeySequence("Ctrl+Shift+C"), [this] {
        auto paths = selection();
        if (paths.isEmpty()) paths << currentPath_;
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
        QDir dir(currentPath_);
        if (dir.cdUp()) navigate(dir.absolutePath());
    }, "fileUp", true);

    connect(page_->findChild<QPushButton*>("fileAssociations"), &QPushButton::clicked, this,
            [this] { showFileAssociationSettings(page_); });
    connect(location_, &QLineEdit::returnPressed, this, [this] { navigate(location_->text()); });
    connect(places_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const auto path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) navigate(path);
    });

    places_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(places_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const bool keyboard = position.x() < 0 || position.y() < 0;
        auto* item = keyboard ? places_->currentItem() : places_->itemAt(position);
        if (!item) return;
        const auto path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) return;
        auto* menu = new QMenu(page_);
        menu->setObjectName("filePlacesContextMenu");
        menu->setAttribute(Qt::WA_DeleteOnClose);
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        menu->addAction(translate("Open"), this, [this, path] { navigate(path); });
        menu->addAction(translate("Open in a new window"), this, [this, path] {
            if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--app", "files", "--builtin", "--path", path}))
                status_->setText(translate("Could not open a new Files window."));
        });
        menu->addAction(translate("Open terminal here"), this, [this, path] { openTerminal(path); });
        const bool pinned = isPinned(path);
        menu->addAction(pinned ? translate("Unpin folder") : translate("Pin folder"), this,
                        [this, path, pinned] { setPinned(path, !pinned); });
        menu->addSeparator();
        menu->addAction(translate("Copy paths"), this, [path] { QApplication::clipboard()->setText(path); });
        menu->addAction(translate("Properties"), this, [this, path] {
            details_->selectionModel()->clear();
            const auto index = model_->index(path);
            if (index.isValid()) details_->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            properties();
        });
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
        sortColumn_ = column;
        ascending_ = order == Qt::AscendingOrder;
        QSettings settings;
        settings.setValue("files/sortColumn", column);
        settings.setValue("files/sortAscending", ascending_);
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
        view->setDragEnabled(true);
        view->setAcceptDrops(true);
        view->viewport()->setAcceptDrops(true);
        view->setDragDropMode(QAbstractItemView::DragDrop);
        view->setDefaultDropAction(Qt::CopyAction);
        view->setMouseTracking(true);
        view->viewport()->setMouseTracking(true);
        view->viewport()->installEventFilter(this);
        connect(view, &QAbstractItemView::customContextMenuRequested, this,
                [this, view](const QPoint& position) { contextMenu(view, position); });
        connect(view, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
            if (!index.isValid() || busy_) return;
            updatePreviewForIndex(index);
            if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) ||
                QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) return;
            const auto info = model_->fileInfo(index);
            if (info.isDir()) navigate(info.absoluteFilePath());
            else openAssociatedFiles(page_, {info.absoluteFilePath()}, false,
                                     [this](const QString& message) { if (!message.isEmpty()) status_->setText(message); });
        });
        connect(view, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& index) {
            if (!index.isValid() || busy_) return;
            const auto info = model_->fileInfo(index);
            if (info.isDir()) navigate(info.absoluteFilePath());
        });
    }

    refreshPlaces();
    navigate(QDir::homePath());
    updatePreview();
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
    model_->setReadOnly(true);
    model_->setNameFilterDisables(false);
    model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System |
                      (hidden_->isChecked() ? QDir::Hidden : QDir::Filters{}));
    const auto path = currentPath_.isEmpty() ? QDir::homePath() : currentPath_;
    model_->setRootPath(path);
    details_->setModel(model_);
    icons_->setModel(model_);
    auto* unusedSelection = icons_->selectionModel();
    icons_->setSelectionModel(details_->selectionModel());
    unusedSelection->deleteLater();
    for (auto* selection : oldSelections)
        if (selection->model() == oldModel) selection->deleteLater();
    if (oldModel) oldModel->deleteLater();
    connect(details_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        updateActions();
        const auto paths = selection();
        if (!paths.isEmpty()) updatePreview(paths.first());
    });
    connect(model_, &QFileSystemModel::directoryLoaded, this, [this](const QString& loaded) {
        if (loaded == currentPath_ && !busy_)
            status_->setText(translate("%1 items").arg(model_->rowCount(details_->rootIndex())));
    });
    details_->setRootIndex(model_->index(path));
    icons_->setRootIndex(model_->index(path));
    details_->setColumnWidth(0, 290);
    model_->setNameFilters(search_->text().isEmpty() ? QStringList{} : QStringList{"*" + search_->text() + "*"});
    sort(sortColumn_, ascending_);
    updateActions();
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
        history_ = history_.mid(0, historyIndex_ + 1);
        history_ << currentPath_;
        ++historyIndex_;
    }
    location_->setText(currentPath_);
    model_->setRootPath(currentPath_);
    details_->setRootIndex(model_->index(currentPath_));
    icons_->setRootIndex(model_->index(currentPath_));
    details_->selectionModel()->clear();
    folderTitle_->setText(currentPath_ == QDir::homePath() ? translate("Home") :
                         info.fileName().isEmpty() ? translate("File system") : info.fileName());
    for (int row = 0; row < places_->count(); ++row)
        places_->item(row)->setSelected(places_->item(row)->data(Qt::UserRole).toString() == currentPath_);
    if (!busy_) status_->setText(translate("%1 items").arg(model_->rowCount(details_->rootIndex())));
    updatePreview(currentPath_);
    updateActions();
}

QStringList FileManagerActions::pinnedFolders() const {
    QStringList result;
    const auto stored = QSettings().value("files/pinnedFolders").toStringList();
    for (const auto& path : stored) {
        const QFileInfo info(path);
        if (info.isDir()) result << info.absoluteFilePath();
    }
    result.removeDuplicates();
    return result;
}

bool FileManagerActions::isPinned(const QString& path) const {
    return pinnedFolders().contains(QFileInfo(path).absoluteFilePath());
}

void FileManagerActions::setPinned(const QString& path, bool pinned) {
    const QFileInfo info(path);
    if (!info.isDir()) return;
    auto folders = pinnedFolders();
    const auto absolute = info.absoluteFilePath();
    if (pinned && !folders.contains(absolute)) folders << absolute;
    if (!pinned) folders.removeAll(absolute);
    QSettings().setValue("files/pinnedFolders", folders);
    refreshPlaces();
    status_->setText(pinned ? translate("Folder pinned.") : translate("Folder unpinned."));
}

void FileManagerActions::refreshPlaces() {
    places_->clear();
    QSet<QString> visiblePaths;
    auto add = [this, &visiblePaths](const QString& label, const QString& path, FileIcon icon) {
        if (path.isEmpty()) return;
        const auto absolute = QFileInfo(path).absoluteFilePath();
        if (visiblePaths.contains(absolute)) return;
        visiblePaths.insert(absolute);
        auto* item = new QListWidgetItem(fileIcon(icon), label, places_);
        item->setData(Qt::UserRole, absolute);
    };
    add(translate("Home"), QDir::homePath(), FileIcon::Home);
    for (const auto& path : pinnedFolders())
        add(QStringLiteral("★ ") + QFileInfo(path).fileName(), path, FileIcon::Folder);
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
    for (const auto& id : {"open", "copy", "cut", "trash", "duplicate", "copyTo", "moveTo", "delete"})
        actions_.value(id)->setEnabled(!busy_ && !paths.isEmpty());
    const bool files = !paths.isEmpty() && std::all_of(paths.begin(), paths.end(), [](const auto& path) { return QFileInfo(path).isFile(); });
    actions_.value("openWith")->setEnabled(!busy_ && files);
    actions_.value("setDefault")->setEnabled(!busy_ && files && paths.size() == 1);
    actions_.value("resetDefault")->setEnabled(!busy_ && files && paths.size() == 1 && !FileAssociations().applicationForFile(paths.first()).isEmpty());
    actions_.value("rename")->setEnabled(!busy_ && paths.size() == 1);
    actions_.value("paste")->setEnabled(!busy_ && !clipboardFiles().isEmpty() && QFileInfo(currentPath_).isWritable());
    for (const auto& id : {"newFile", "newFolder"})
        actions_.value(id)->setEnabled(!busy_ && QFileInfo(currentPath_).isWritable());
    actions_.value("back")->setEnabled(historyIndex_ > 0);
    actions_.value("forward")->setEnabled(historyIndex_ + 1 < history_.size());
}

void FileManagerActions::openSelection(bool chooseApplication) {
    const auto paths = selection();
    if (paths.isEmpty() || busy_) return;
    if (paths.size() == 1 && QFileInfo(paths.first()).isDir()) {
        navigate(paths.first());
        return;
    }
    openAssociatedFiles(page_, paths, chooseApplication,
                        [this](const QString& message) { if (!message.isEmpty()) status_->setText(message); });
}

void FileManagerActions::contextMenu(QAbstractItemView* view, const QPoint& position) {
    QModelIndex index;
    const bool keyboard = position.x() < 0 || position.y() < 0;
    if (keyboard) index = view->currentIndex();
    else index = view->indexAt(position);
    if (index.isValid() && !view->selectionModel()->isSelected(index.siblingAtColumn(0)))
        view->selectionModel()->setCurrentIndex(index.siblingAtColumn(0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    else if (!index.isValid() && !keyboard)
        view->selectionModel()->clear();
    updateActions();
    const auto paths = selection();
    const bool directory = paths.size() == 1 && QFileInfo(paths.first()).isDir();
    const QString destination = directory ? paths.first() : currentPath_;
    auto* menu = new QMenu(page_);
    menu->setObjectName("fileContextMenu");
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    if (!paths.isEmpty()) {
        menu->addAction(actions_.value("open"));
        if (!directory) {
            auto* openWithMenu = menu->addMenu(translate("Open with"));
            openWithMenu->addAction(actions_.value("openWith"));
            openWithMenu->addSeparator();
            openWithMenu->addAction(actions_.value("setDefault"));
            openWithMenu->addAction(actions_.value("resetDefault"));
        }
        if (directory) {
            menu->addAction(translate("Open in a new window"), this, [this, destination] {
                if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--app", "files", "--builtin", "--path", destination}))
                    status_->setText(translate("Could not open a new Files window."));
            });
            const bool pinned = isPinned(destination);
            menu->addAction(pinned ? translate("Unpin folder") : translate("Pin folder"), this,
                            [this, destination, pinned] { setPinned(destination, !pinned); });
            menu->addAction(translate("Open terminal here"), this, [this, destination] { openTerminal(destination); });
        }
        menu->addSeparator();
        menu->addAction(actions_.value("cut"));
        menu->addAction(actions_.value("copy"));
        menu->addAction(actions_.value("duplicate"));
        auto* organize = menu->addMenu(translate("Organize"));
        organize->addAction(actions_.value("copyTo"));
        organize->addAction(actions_.value("moveTo"));
        organize->addSeparator();
        organize->addAction(actions_.value("rename"));
        organize->addAction(actions_.value("trash"));
        organize->addAction(actions_.value("delete"));
        menu->addSeparator();
    }

    auto* pasteAction = menu->addAction(directory ? translate("Paste into this folder") : translate("Paste"), this,
                                        [this, destination] { paste(destination); });
    pasteAction->setEnabled(!busy_ && !clipboardFiles().isEmpty() && QFileInfo(destination).isWritable());
    if (paths.isEmpty() || directory) {
        auto* create = menu->addMenu(translate("Create"));
        auto* newFile = create->addAction(translate("New file"), this, [this, destination] { createEntry(false, destination); });
        auto* newFolder = create->addAction(translate("New folder"), this, [this, destination] { createEntry(true, destination); });
        newFile->setEnabled(!busy_ && QFileInfo(destination).isWritable());
        newFolder->setEnabled(newFile->isEnabled());
        if (paths.isEmpty()) menu->addAction(translate("Open terminal here"), this, [this, destination] { openTerminal(destination); });
    }

    menu->addAction(actions_.value("copyPath"));
    menu->addAction(actions_.value("properties"));
    menu->addSeparator();
    auto* viewMenu = menu->addMenu(translate("View"));
    auto* sorting = viewMenu->addMenu(translate("Sort by"));
    const QStringList labels{translate("Name"), translate("Size"), translate("Type"), translate("Modified")};
    for (int column = 0; column < labels.size(); ++column) {
        auto* item = sorting->addAction(labels.at(column), this, [this, column] { sort(column, ascending_); });
        item->setCheckable(true);
        item->setChecked(sortColumn_ == column);
    }
    sorting->addSeparator();
    auto* descending = sorting->addAction(translate("Descending order"), this, [this] { sort(sortColumn_, !ascending_); });
    descending->setCheckable(true);
    descending->setChecked(!ascending_);
    auto* hiddenAction = viewMenu->addAction(translate("Show hidden files"), hidden_, &QPushButton::click);
    hiddenAction->setCheckable(true);
    hiddenAction->setChecked(hidden_->isChecked());
    viewMenu->addAction(actions_.value("refresh"));
    menu->addAction(translate("File associations"), this, [this] { showFileAssociationSettings(page_); });
    const auto point = keyboard ? (index.isValid() ? view->visualRect(index).center() : QPoint(16, 16)) : position;
    menu->popup(view->viewport()->mapToGlobal(point));
}

void FileManagerActions::copySelection(bool cut) {
    const auto paths = selection();
    if (paths.isEmpty() || busy_) return;
    auto* mime = new QMimeData;
    QList<QUrl> urls;
    QByteArray gnome = cut ? "cut\n" : "copy\n";
    for (const auto& path : paths) {
        const auto url = QUrl::fromLocalFile(path);
        urls << url;
        gnome += url.toEncoded() + '\n';
    }
    mime->setUrls(urls);
    mime->setData("application/x-kde-cutselection", cut ? "1" : "0");
    mime->setData("x-special/gnome-copied-files", gnome);
    QApplication::clipboard()->setMimeData(mime);
    status_->setText(cut ? translate("Selection cut. Choose a folder and Paste.") :
                           translate("Selection copied. Choose a folder and Paste."));
}

QStringList FileManagerActions::clipboardFiles() const {
    return localFiles(QApplication::clipboard()->mimeData());
}

void FileManagerActions::paste(const QString& destination) {
    const auto paths = clipboardFiles();
    const bool cut = clipboardCut(QApplication::clipboard()->mimeData());
    if (!paths.isEmpty()) run(cut ? "move" : "copy", paths, destination, cut);
}

void FileManagerActions::run(const QString& operation, const QStringList& paths,
                             const QString& destination, bool clearClipboard) {
    if (busy_) return;
    busy_ = true;
    updateActions();
    status_->setText(translate("Working..."));
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [=, this] {
        const auto error = watcher->result();
        busy_ = false;
        if (error.isEmpty() && clearClipboard && clipboardFiles() == paths &&
            clipboardCut(QApplication::clipboard()->mimeData()))
            QApplication::clipboard()->clear();
        status_->setText(error.isEmpty() ? translate("Operation completed.") : error);
        updateActions();
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([operation, paths, destination] {
        return performFileOperation(operation, paths, destination);
    }));
}

void FileManagerActions::createEntry(bool folder, const QString& destination) {
    if (busy_) return;
    auto* dialog = new QInputDialog(page_);
    configureDialog(dialog);
    dialog->setWindowTitle(folder ? translate("New folder") : translate("New file"));
    dialog->setLabelText(folder ? translate("Folder name") : translate("File name"));
    connect(dialog, &QInputDialog::textValueSelected, this, [this, destination, folder](const QString& name) {
        if (busy_) return;
        bool success = false;
        if (validFileName(name)) {
            if (folder) success = QDir(destination).mkdir(name);
            else {
                QFile file(QDir(destination).filePath(name));
                success = file.open(QIODevice::WriteOnly | QIODevice::NewOnly);
            }
        }
        status_->setText(success ? translate("Operation completed.") :
                         translate("Could not create the item. Check its name and permissions; existing files are never overwritten."));
    });
    dialog->open();
}

void FileManagerActions::renameSelection() {
    const auto paths = selection();
    if (busy_ || paths.size() != 1) return;
    const QFileInfo info(paths.first());
    auto* dialog = new QInputDialog(page_);
    configureDialog(dialog);
    dialog->setWindowTitle(translate("Rename"));
    dialog->setLabelText(translate("New name"));
    dialog->setTextValue(info.fileName());
    connect(dialog, &QInputDialog::textValueSelected, this, [this, info](const QString& name) {
        if (busy_ || name == info.fileName()) return;
        if (!validFileName(name) || QFileInfo::exists(info.dir().filePath(name)) ||
            QFileInfo(info.dir().filePath(name)).isSymLink() ||
            !QDir().rename(info.absoluteFilePath(), info.dir().filePath(name)))
            status_->setText(translate("Could not rename item. Nothing was overwritten."));
    });
    dialog->open();
}

void FileManagerActions::trashSelection() {
    const auto paths = selection();
    if (busy_ || paths.isEmpty()) return;
    auto* confirm = new QMessageBox(QMessageBox::Question, translate("Move to Trash"),
        translate("Move %1 selected items to Trash?").arg(paths.size()),
        QMessageBox::Yes | QMessageBox::Cancel, page_);
    configureDialog(confirm);
    confirm->setTextFormat(Qt::PlainText);
    confirm->setDefaultButton(QMessageBox::Cancel);
    connect(confirm, &QMessageBox::finished, this, [this, paths](int result) {
        if (result == QMessageBox::Yes) run("trash", paths, {});
    });
    confirm->open();
}

void FileManagerActions::deleteSelectionPermanently() {
    const auto paths = selection();
    if (busy_ || paths.isEmpty()) return;
    auto* confirm = new QMessageBox(QMessageBox::Warning, translate("Delete permanently"),
        translate("Permanently delete %1 selected items? This cannot be undone.").arg(paths.size()),
        QMessageBox::Yes | QMessageBox::Cancel, page_);
    configureDialog(confirm);
    confirm->setTextFormat(Qt::PlainText);
    confirm->setDefaultButton(QMessageBox::Cancel);
    connect(confirm, &QMessageBox::finished, this, [this, paths](int result) {
        if (result == QMessageBox::Yes) run("delete", paths, {});
    });
    confirm->open();
}

void FileManagerActions::duplicateSelection() {
    const auto paths = selection();
    if (busy_ || paths.isEmpty()) return;
    run("duplicate", paths, {});
}

void FileManagerActions::copyOrMoveSelection(bool move) {
    const auto paths = selection();
    if (busy_ || paths.isEmpty()) return;
    auto* dialog = new QFileDialog(page_, move ? translate("Move to…") : translate("Copy to…"), currentPath_);
    configureDialog(dialog);
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setLabelText(QFileDialog::Accept, move ? translate("Move here") : translate("Copy here"));
    connect(dialog, &QFileDialog::fileSelected, this, [this, paths, move](const QString& directory) {
        if (!directory.isEmpty()) run(move ? "move" : "copy", paths, directory);
    });
    dialog->open();
}

void FileManagerActions::openTerminal(const QString& directory) {
    QString error;
    auto command = defaultApplicationCommand("terminal", &error);
    if (!error.isEmpty() || command.isEmpty()) {
        status_->setText(error.isEmpty() ? translate("No default terminal is configured.") : error);
        return;
    }
    QProcess process;
    process.setProgram(command.takeFirst());
    process.setArguments(command);
    process.setWorkingDirectory(directory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PWD", directory);
    process.setProcessEnvironment(environment);
    if (!process.startDetached())
        status_->setText(translate("Could not start the terminal: %1").arg(process.errorString()));
}

void FileManagerActions::properties() {
    auto paths = selection();
    if (paths.isEmpty()) paths << currentPath_;
    auto* dialog = new QDialog(page_);
    configureDialog(dialog);
    dialog->setWindowTitle(translate("Properties"));
    dialog->resize(640, 470);
    auto* layout = new QVBoxLayout(dialog);
    auto* text = new QPlainTextEdit(dialog);
    text->setReadOnly(true);
    QStringList sections;
    for (const auto& path : paths) {
        const QFileInfo info(path);
        QStringList lines{
            translate("Name") + ": " + (info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName()),
            translate("Location") + ": " + path,
            translate("Type") + ": " + (info.isDir() ? translate("Folder") : FileAssociations::mimeTypeForFile(path)),
            translate("Size") + ": " + (info.isDir() ? translate("Folder contents are not included in this size.") : QLocale().formattedDataSize(info.size())),
            translate("Created") + ": " + prettyDate(info.birthTime()),
            translate("Modified") + ": " + prettyDate(info.lastModified()),
            translate("Last accessed") + ": " + prettyDate(info.lastRead()),
            translate("Owner") + ": " + info.owner(),
            translate("Group") + ": " + info.group(),
            translate("Readable") + ": " + (info.isReadable() ? translate("Yes") : translate("No")),
            translate("Writable") + ": " + (info.isWritable() ? translate("Yes") : translate("No")),
            translate("Executable") + ": " + (info.isExecutable() ? translate("Yes") : translate("No"))
        };
        if (info.isSymLink()) lines << translate("Symbolic link target") + ": " + info.symLinkTarget();
        sections << lines.join('\n');
    }
    text->setPlainText(sections.join("\n\n"));
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    dialog->open();
}

void FileManagerActions::sort(int column, bool ascending) {
    sortColumn_ = qBound(0, column, 3);
    ascending_ = ascending;
    details_->sortByColumn(sortColumn_, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
    model_->sort(sortColumn_, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void FileManagerActions::updatePreviewForIndex(const QModelIndex& index) {
    if (!index.isValid()) return;
    updatePreview(model_->filePath(index));
}

void FileManagerActions::updatePreview(const QString& path) {
    if (!previewImage_ || !previewTitle_ || !previewMeta_) return;
    const QString target = path.isEmpty() ? currentPath_ : path;
    const QFileInfo info(target);
    previewPath_ = info.absoluteFilePath();
    previewImage_->setPixmap({});
    previewTitle_->setText(info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName());

    if (!info.exists() && !info.isSymLink()) {
        previewImage_->setText(translate("Nothing to preview."));
        previewMeta_->clear();
        return;
    }

    const QString mime = info.isDir() ? QStringLiteral("inode/directory") : FileAssociations::mimeTypeForFile(target);
    QStringList meta{
        info.isDir() ? translate("Folder") : mime,
        info.isDir() ? QString() : QLocale().formattedDataSize(info.size()),
        translate("Modified") + ": " + prettyDate(info.lastModified())
    };
    meta.removeAll(QString());
    previewMeta_->setText(meta.join("\n"));

    if (info.isDir()) {
        previewImage_->setPixmap(fileIcon(FileIcon::Folder).pixmap(128, 128));
        previewHint_->setText(isPinned(target) ? translate("Pinned folder") : translate("Right-click to pin this folder."));
        return;
    }

    if (mime.startsWith("image/")) {
        QImageReader reader(target);
        reader.setAutoTransform(true);
        const auto size = previewImage_->size().expandedTo(QSize(240, 190));
        reader.setScaledSize(reader.size().scaled(size, Qt::KeepAspectRatio));
        const QImage image = reader.read();
        if (!image.isNull()) {
            previewImage_->setPixmap(QPixmap::fromImage(image).scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            previewImage_->setText({});
            previewHint_->setText(translate("Image preview"));
            return;
        }
    }

    if (mime.startsWith("video/")) {
        const QString cache = previewCachePath(info);
        if (QFileInfo::exists(cache)) {
            const QPixmap thumbnail(cache);
            if (!thumbnail.isNull()) {
                previewImage_->setPixmap(thumbnail.scaled(previewImage_->size().expandedTo(QSize(240, 190)),
                                                          Qt::KeepAspectRatio, Qt::SmoothTransformation));
                previewImage_->setText({});
                previewHint_->setText(translate("Video thumbnail preview"));
                return;
            }
        }

        QString program = QStandardPaths::findExecutable("ffmpegthumbnailer");
        QStringList arguments;
        if (!program.isEmpty()) {
            arguments = {"-i", target, "-o", cache, "-s", "512", "-q", "8"};
        } else {
            program = QStandardPaths::findExecutable("ffmpeg");
            if (!program.isEmpty())
                arguments = {"-y", "-ss", "00:00:01", "-i", target, "-frames:v", "1", "-vf", "scale=512:-1", cache};
        }

        if (!program.isEmpty()) {
            previewImage_->setPixmap(fileIcon(FileIcon::Video).pixmap(128, 128));
            previewHint_->setText(translate("Generating video preview…"));
            auto* process = new QProcess(this);
            process->setProgram(program);
            process->setArguments(arguments);
            connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                    [this, process, cache, target](int code, QProcess::ExitStatus status) {
                process->deleteLater();
                if (previewPath_ != QFileInfo(target).absoluteFilePath()) return;
                if (status == QProcess::NormalExit && code == 0 && QFileInfo::exists(cache)) {
                    const QPixmap thumbnail(cache);
                    if (!thumbnail.isNull()) {
                        previewImage_->setPixmap(thumbnail.scaled(previewImage_->size().expandedTo(QSize(240, 190)),
                                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        previewImage_->setText({});
                        previewHint_->setText(translate("Video thumbnail preview"));
                    }
                }
            });
            process->start();
            return;
        }
        previewImage_->setPixmap(fileIcon(FileIcon::Video).pixmap(128, 128));
        previewHint_->setText(translate("Install ffmpeg or ffmpegthumbnailer for video previews."));
        return;
    }

    previewImage_->setPixmap(fileIcon(FileIcon::File).pixmap(112, 112));
    previewHint_->setText(translate("Hover another file to preview it without opening it."));
}

bool FileManagerActions::eventFilter(QObject* watched, QEvent* event) {
    auto* view = watched == details_->viewport() ? static_cast<QAbstractItemView*>(details_) :
                 watched == icons_->viewport() ? static_cast<QAbstractItemView*>(icons_) : nullptr;
    if (!view) return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::MouseMove) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const auto index = view->indexAt(mouse->position().toPoint());
        if (index.isValid()) updatePreviewForIndex(index);
        return QObject::eventFilter(watched, event);
    }

    if (event->type() != QEvent::DragEnter && event->type() != QEvent::DragMove && event->type() != QEvent::Drop)
        return QObject::eventFilter(watched, event);

    auto* drop = static_cast<QDropEvent*>(event);
    const auto paths = localFiles(drop->mimeData());
    const auto index = view->indexAt(drop->position().toPoint());
    const auto destination = index.isValid() && model_->isDir(index) ? model_->filePath(index) : currentPath_;
    if (busy_ || paths.isEmpty() || !QFileInfo(destination).isWritable()) {
        drop->ignore();
        return true;
    }
    const bool move = drop->modifiers().testFlag(Qt::ShiftModifier);
    const auto action = move ? Qt::MoveAction : Qt::CopyAction;
    if (!drop->possibleActions().testFlag(action)) {
        drop->ignore();
        return true;
    }
    drop->setDropAction(action);
    drop->accept();
    if (event->type() == QEvent::Drop) run(move ? "move" : "copy", paths, destination);
    return true;
}
} // namespace LuDash
