#include <LuDash/localization/Localization.h>
#include <LuDash/file_manager/FileManager.h>
#include <LuDash/file_operations/FileOperations.h>
#include <QtWidgets>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <memory>
namespace LuDash {
QWidget* createFileManager() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page); layout->setContentsMargins(20, 18, 20, 14); layout->setSpacing(14);
    auto* header = new QHBoxLayout;
    auto* title = new QLabel(translate("Files")); title->setObjectName("heading");
    header->addWidget(title); header->addStretch();
    auto* hidden = new QPushButton(translate("Hidden files")); hidden->setCheckable(true); header->addWidget(hidden);
    auto* mode = new QPushButton(translate("Icon view")); mode->setCheckable(true); header->addWidget(mode); layout->addLayout(header);
    auto* navigation = new QHBoxLayout;
    auto* back = new QPushButton("←"); back->setToolTip(translate("Back")); back->setObjectName("fileBack");
    auto* forward = new QPushButton("→"); forward->setToolTip(translate("Forward"));
    auto* up = new QPushButton("↑"); up->setToolTip(translate("Up"));
    auto* location = new QLineEdit(QDir::homePath()); location->setObjectName("fileLocation");
    auto* search = new QLineEdit; search->setPlaceholderText(translate("Filter this folder")); search->setMaximumWidth(210);
    navigation->addWidget(back); navigation->addWidget(forward); navigation->addWidget(up); navigation->addWidget(location, 1); navigation->addWidget(search); layout->addLayout(navigation);
    auto* actions = new QHBoxLayout;
    auto addButton = [actions](const char* label, const char* name) { auto* button = new QPushButton(translate(label)); button->setObjectName(name); actions->addWidget(button); return button; };
    auto* newFolder = addButton("New folder", "fileNewFolder"); auto* copy = addButton("Copy", "fileCopy"); auto* cut = addButton("Cut", "fileCut"); auto* paste = addButton("Paste", "filePaste"); auto* rename = addButton("Rename", "fileRename"); auto* trash = addButton("Move to Trash", "fileTrash"); actions->addStretch(); layout->addLayout(actions);
    auto* splitter = new QSplitter; auto* places = new QListWidget; places->setObjectName("filePlaces"); places->setMinimumWidth(130); places->setMaximumWidth(240);
    auto addPlace = [places, page](const char* label, const QString& path, QStyle::StandardPixmap icon) { if (path.isEmpty()) return; auto* item = new QListWidgetItem(page->style()->standardIcon(icon), translate(label), places); item->setData(Qt::UserRole, path); };
    addPlace("Home", QDir::homePath(), QStyle::SP_DirHomeIcon);
    addPlace("Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), QStyle::SP_DesktopIcon);
    addPlace("Documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), QStyle::SP_DirIcon);
    addPlace("Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), QStyle::SP_DirIcon);
    addPlace("Pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), QStyle::SP_DirIcon);
    addPlace("Music", QStandardPaths::writableLocation(QStandardPaths::MusicLocation), QStyle::SP_MediaVolume);
    addPlace("Videos", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), QStyle::SP_MediaPlay);
    addPlace("File system", "/", QStyle::SP_DriveHDIcon);
    for (const auto& volume : QStorageInfo::mountedVolumes()) if (volume.isValid() && volume.isReady() && volume.rootPath() != "/" && (volume.rootPath().startsWith("/run/media/") || volume.rootPath().startsWith("/media/") || volume.rootPath().startsWith("/mnt/"))) {
        auto* item = new QListWidgetItem(page->style()->standardIcon(QStyle::SP_DriveHDIcon), volume.displayName(), places); item->setData(Qt::UserRole, volume.rootPath());
    }
    auto* model = new QFileSystemModel(page); model->setRootPath(QDir::homePath()); model->setReadOnly(true);
    auto* view = new QTreeView; view->setObjectName("fileView"); view->setModel(model); view->setRootIsDecorated(false); view->setSortingEnabled(true); view->sortByColumn(0, Qt::AscendingOrder); view->setColumnWidth(0, 290); view->setSelectionMode(QAbstractItemView::ExtendedSelection); view->setAlternatingRowColors(true);
    auto* icons = new QListView; icons->setObjectName("fileIcons"); icons->setModel(model); icons->setViewMode(QListView::IconMode); icons->setResizeMode(QListView::Adjust); icons->setMovement(QListView::Static); icons->setIconSize({48, 48}); icons->setGridSize({128, 100}); icons->setWordWrap(true); icons->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto* stack = new QStackedWidget; stack->addWidget(view); stack->addWidget(icons); splitter->addWidget(places); splitter->addWidget(stack); splitter->setStretchFactor(1, 1); splitter->setSizes({160, 700}); layout->addWidget(splitter, 1);
    auto* status = new QLabel; status->setObjectName("fileStatus"); status->setWordWrap(true); layout->addWidget(status);
    struct Navigation { QStringList history; int index = -1; QStringList clipboard; bool cut = false; };
    auto state = std::make_shared<Navigation>();
    auto navigate = [=](const QString& path, bool remember = true) {
        const QFileInfo info(path);
        if (!info.isDir()) { status->setText(translate("Folder not found: ") + path); return; }
        const auto absolute = info.absoluteFilePath();
        if (remember && (state->index < 0 || state->history.value(state->index) != absolute)) { state->history = state->history.mid(0, state->index + 1); state->history << absolute; state->index++; }
        location->setText(absolute); const auto index = model->index(absolute); view->setRootIndex(index); icons->setRootIndex(index);
        back->setEnabled(state->index > 0); forward->setEnabled(state->index + 1 < state->history.size()); status->setText(absolute);
    };
    navigate(QDir::homePath());
    QObject::connect(location, &QLineEdit::returnPressed, page, [=] { navigate(location->text()); });
    QObject::connect(back, &QPushButton::clicked, page, [=] { if (state->index > 0) navigate(state->history[--state->index], false); });
    QObject::connect(forward, &QPushButton::clicked, page, [=] { if (state->index + 1 < state->history.size()) navigate(state->history[++state->index], false); });
    QObject::connect(up, &QPushButton::clicked, page, [=] { QDir dir(location->text()); dir.cdUp(); navigate(dir.path()); });
    QObject::connect(places, &QListWidget::itemClicked, page, [=](QListWidgetItem* item) { navigate(item->data(Qt::UserRole).toString()); });
    QObject::connect(mode, &QPushButton::toggled, stack, [=](bool checked) { stack->setCurrentIndex(checked ? 1 : 0); });
    QObject::connect(hidden, &QPushButton::toggled, model, [=](bool checked) { model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | (checked ? QDir::Hidden : QDir::Filters{})); });
    QObject::connect(search, &QLineEdit::textChanged, model, [=](const QString& value) { model->setNameFilters(value.isEmpty() ? QStringList{} : QStringList{"*" + value + "*"}); model->setNameFilterDisables(false); });
    auto open = [=](const QModelIndex& index) { const auto info = model->fileInfo(index); if (info.isDir()) navigate(info.absoluteFilePath()); else if (info.isExecutable()) status->setText(translate("Executable files are not launched from Files. Use the terminal to run trusted programs.")); else if (!QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()))) status->setText(translate("Unable to open this file.")); };
    QObject::connect(view, &QTreeView::doubleClicked, page, open); QObject::connect(icons, &QListView::doubleClicked, page, open);
    auto selection = [=] { QStringList paths; auto* active = mode->isChecked() ? static_cast<QAbstractItemView*>(icons) : static_cast<QAbstractItemView*>(view); for (const auto& index : active->selectionModel()->selectedIndexes()) if (index.column() == 0) paths << model->filePath(index); return paths; };
    auto* watcher = new QFutureWatcher<QString>(page);
    auto run = [=](const QString& operation, const QStringList& paths) { if (watcher->isRunning()) return; status->setText(translate("Working...")); paste->setEnabled(false); trash->setEnabled(false); watcher->setFuture(QtConcurrent::run(performFileOperation, operation, paths, location->text())); };
    QObject::connect(watcher, &QFutureWatcher<QString>::finished, page, [=] { status->setText(watcher->result().isEmpty() ? translate("Operation completed.") : watcher->result()); paste->setEnabled(true); trash->setEnabled(true); });
    QObject::connect(copy, &QPushButton::clicked, page, [=] { state->clipboard = selection(); state->cut = false; status->setText(translate("Selection copied. Choose a folder and Paste.")); });
    QObject::connect(cut, &QPushButton::clicked, page, [=] { state->clipboard = selection(); state->cut = true; status->setText(translate("Selection cut. Choose a folder and Paste.")); });
    QObject::connect(paste, &QPushButton::clicked, page, [=] { run(state->cut ? "move" : "copy", state->clipboard); });
    QObject::connect(trash, &QPushButton::clicked, page, [=] { const auto paths = selection(); if (!paths.isEmpty() && QMessageBox::question(page, translate("Move to Trash"), QString(translate("Move %1 selected items to Trash?")).arg(paths.size())) == QMessageBox::Yes) run("trash", paths); });
    QObject::connect(newFolder, &QPushButton::clicked, page, [=] { bool ok = false; const auto name = QInputDialog::getText(page, translate("New folder"), translate("Folder name"), QLineEdit::Normal, "", &ok); if (!ok) return; if (!validFileName(name) || !QDir(location->text()).mkdir(name)) status->setText(translate("Could not create folder. Check its name and permissions.")); });
    QObject::connect(rename, &QPushButton::clicked, page, [=] { const auto paths = selection(); if (paths.size() != 1) { status->setText(translate("Select one item to rename.")); return; } const QFileInfo info(paths.first()); bool ok = false; const auto name = QInputDialog::getText(page, translate("Rename"), translate("New name"), QLineEdit::Normal, info.fileName(), &ok); if (!ok || name == info.fileName()) return; if (!validFileName(name) || !QDir().rename(paths.first(), info.dir().filePath(name))) status->setText(translate("Could not rename item. Nothing was overwritten.")); });
    auto shortcut = [page](const QKeySequence& sequence, const std::function<void()>& action) { auto* key = new QAction(page); key->setShortcut(sequence); key->setShortcutContext(Qt::WidgetWithChildrenShortcut); page->addAction(key); QObject::connect(key, &QAction::triggered, page, action); };
    shortcut(QKeySequence("Alt+Left"), [back] { back->click(); }); shortcut(QKeySequence("Alt+Right"), [forward] { forward->click(); }); shortcut(QKeySequence("Alt+Up"), [up] { up->click(); }); shortcut(QKeySequence("Ctrl+L"), [location] { location->setFocus(); location->selectAll(); }); shortcut(QKeySequence("F2"), [rename] { rename->click(); });
    return page;
}
}
