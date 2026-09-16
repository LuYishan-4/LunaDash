#include <LuDash/localization/Localization.h>
#include <LuDash/file_manager/FileManager.h>
#include <LuDash/file_operations/FileOperations.h>
#include <LuDash/file_icons/FileIcons.h>
#include <LuDash/file_icons/FileIconDelegate.h>
#include <LuDash/default_applications/DefaultApplications.h>
#include <QtWidgets>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>
#include <QProcess>
#include <memory>
namespace LuDash {
QWidget* createFileManager() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page); layout->setContentsMargins(24, 20, 24, 14); layout->setSpacing(18);
    auto button = [page](FileIcon icon, const char* label, const char* name) { auto* control = new QPushButton(fileIcon(icon), "", page); control->setObjectName(name); control->setProperty("fileTool", true); control->setToolTip(translate(label)); control->setAccessibleName(translate(label)); control->setIconSize({20, 20}); control->setFixedSize(36, 36); control->setCursor(Qt::PointingHandCursor); return control; };
    auto* header = new QHBoxLayout; header->setSpacing(12);
    auto* brand = new QLabel; brand->setPixmap(fileIcon(FileIcon::Folder).pixmap(26, 26));
    auto* title = new QLabel(translate("Files")); title->setObjectName("fileBrand");
    header->addWidget(brand); header->addWidget(title); header->addStretch();
    auto* search = new QLineEdit; search->setObjectName("fileSearch"); search->setPlaceholderText(translate("Search this folder")); search->setFixedWidth(230); search->setMinimumHeight(36); search->addAction(fileIcon(FileIcon::Search), QLineEdit::LeadingPosition); header->addWidget(search); layout->addLayout(header);
    auto* navigation = new QHBoxLayout; navigation->setSpacing(6);
    auto* back = button(FileIcon::Back, "Back", "fileBack"); auto* forward = button(FileIcon::Forward, "Forward", "fileForward"); auto* up = button(FileIcon::Up, "Up", "fileUp");
    auto* location = new QLineEdit(QDir::homePath()); location->setObjectName("fileLocation"); location->setMinimumHeight(38); location->addAction(fileIcon(FileIcon::Folder), QLineEdit::LeadingPosition);
    auto* hidden = button(FileIcon::Eye, "Show hidden files", "fileHidden"); hidden->setCheckable(true);
    auto* mode = button(FileIcon::Grid, "Switch view", "fileMode"); mode->setCheckable(true); mode->setChecked(true);
    navigation->addWidget(back); navigation->addWidget(forward); navigation->addWidget(up); navigation->addSpacing(8); navigation->addWidget(location, 1); navigation->addSpacing(8); navigation->addWidget(hidden); navigation->addWidget(mode); layout->addLayout(navigation);
    auto* splitter = new QSplitter; splitter->setHandleWidth(22);
    auto* sidebar = new QWidget; auto* sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(0, 8, 0, 0); sideLayout->setSpacing(12);
    auto* sideTitle = new QLabel(translate("LIBRARY")); sideTitle->setObjectName("fileSection"); sideTitle->setContentsMargins(14, 0, 0, 0); sideLayout->addWidget(sideTitle);
    auto* places = new QListWidget; places->setObjectName("filePlaces"); places->setIconSize({20, 20}); places->setSpacing(4); sideLayout->addWidget(places);
    sidebar->setMinimumWidth(145); sidebar->setMaximumWidth(200);
    auto addPlace = [places](const char* label, const QString& path, FileIcon icon) { if (path.isEmpty()) return; auto* item = new QListWidgetItem(fileIcon(icon), translate(label), places); item->setData(Qt::UserRole, path); };
    addPlace("Home", QDir::homePath(), FileIcon::Home);
    addPlace("Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), FileIcon::Desktop);
    addPlace("Documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), FileIcon::File);
    addPlace("Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), FileIcon::Download);
    addPlace("Pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), FileIcon::Picture);
    addPlace("Music", QStandardPaths::writableLocation(QStandardPaths::MusicLocation), FileIcon::Music);
    addPlace("Videos", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), FileIcon::Video);
    addPlace("File system", "/", FileIcon::Drive);
    for (const auto& volume : QStorageInfo::mountedVolumes()) if (volume.isValid() && volume.isReady() && volume.rootPath() != "/" && (volume.rootPath().startsWith("/run/media/") || volume.rootPath().startsWith("/media/") || volume.rootPath().startsWith("/mnt/"))) {
        auto* item = new QListWidgetItem(fileIcon(FileIcon::Drive), volume.displayName(), places); item->setData(Qt::UserRole, volume.rootPath());
    }
    auto* browser = new QWidget; auto* browserLayout = new QVBoxLayout(browser); browserLayout->setContentsMargins(0, 0, 0, 0); browserLayout->setSpacing(14);
    auto* actions = new QHBoxLayout; actions->setSpacing(5);
    auto* folderTitle = new QLabel(translate("Home")); folderTitle->setObjectName("fileFolderTitle"); actions->addWidget(folderTitle); actions->addStretch();
    auto* newFile = new QPushButton(fileIcon(FileIcon::File), translate("New file")); newFile->setObjectName("fileNewFile"); newFile->setIconSize({17, 17}); actions->addWidget(newFile);
    auto* newFolder = new QPushButton(fileIcon(FileIcon::Plus), translate("New folder")); newFolder->setObjectName("fileNewFolder"); newFolder->setIconSize({17, 17}); actions->addWidget(newFolder);
    auto* terminal = new QPushButton(fileIcon(FileIcon::Terminal), translate("Open terminal here")); terminal->setObjectName("fileTerminal"); terminal->setIconSize({17, 17}); actions->addWidget(terminal); actions->addSpacing(8);
    auto addButton = [actions, button](FileIcon icon, const char* label, const char* name) { auto* control = button(icon, label, name); actions->addWidget(control); return control; };
    auto* copy = addButton(FileIcon::Copy, "Copy", "fileCopy"); auto* cut = addButton(FileIcon::Cut, "Cut", "fileCut"); auto* paste = addButton(FileIcon::Paste, "Paste", "filePaste"); auto* rename = addButton(FileIcon::Rename, "Rename", "fileRename"); auto* trash = addButton(FileIcon::Trash, "Move to Trash", "fileTrash"); browserLayout->addLayout(actions);
    auto* model = new QFileSystemModel(page); model->setRootPath(QDir::homePath()); model->setReadOnly(true);
    auto* view = new QTreeView; view->setObjectName("fileView"); view->setModel(model); view->setRootIsDecorated(false); view->setSortingEnabled(true); view->sortByColumn(0, Qt::AscendingOrder); view->setColumnWidth(0, 290); view->setSelectionMode(QAbstractItemView::ExtendedSelection); view->setAlternatingRowColors(false); view->setItemDelegate(new FileIconDelegate(view)); view->setIconSize({22, 22}); view->header()->setStretchLastSection(true);
    auto* icons = new QListView; icons->setObjectName("fileIcons"); icons->setModel(model); icons->setViewMode(QListView::IconMode); icons->setResizeMode(QListView::Adjust); icons->setMovement(QListView::Static); icons->setItemDelegate(new FileIconDelegate(icons)); icons->setIconSize({52, 52}); icons->setGridSize({140, 116}); icons->setSpacing(8); icons->setWordWrap(true); icons->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto* stack = new QStackedWidget; stack->addWidget(view); stack->addWidget(icons); stack->setCurrentIndex(1); browserLayout->addWidget(stack, 1); splitter->addWidget(sidebar); splitter->addWidget(browser); splitter->setStretchFactor(1, 1); splitter->setSizes({170, 780}); layout->addWidget(splitter, 1);
    auto* status = new QLabel; status->setObjectName("fileStatus"); status->setWordWrap(true); layout->addWidget(status);
    struct Navigation { QStringList history; int index = -1; QStringList clipboard; bool cut = false; };
    auto state = std::make_shared<Navigation>();
    auto navigate = [=](const QString& path, bool remember = true) {
        const QFileInfo info(path);
        if (!info.isDir()) { status->setText(translate("Folder not found: ") + path); return; }
        const auto absolute = info.absoluteFilePath();
        if (remember && (state->index < 0 || state->history.value(state->index) != absolute)) { state->history = state->history.mid(0, state->index + 1); state->history << absolute; state->index++; }
        model->sort(0, Qt::AscendingOrder);
        location->setText(absolute); const auto index = model->index(absolute); view->setRootIndex(index); icons->setRootIndex(index);
        back->setEnabled(state->index > 0); forward->setEnabled(state->index + 1 < state->history.size()); folderTitle->setText(absolute == QDir::homePath() ? translate("Home") : info.fileName().isEmpty() ? translate("File system") : info.fileName());
        for (int row = 0; row < places->count(); ++row) places->item(row)->setSelected(places->item(row)->data(Qt::UserRole).toString() == absolute);
        status->setText(QString(translate("%1 items")).arg(model->rowCount(index)));
    };
    QObject::connect(model, &QFileSystemModel::directoryLoaded, page, [=](const QString& path) { if (path == location->text()) status->setText(QString(translate("%1 items")).arg(model->rowCount(view->rootIndex()))); });
    navigate(QDir::homePath());
    QObject::connect(location, &QLineEdit::returnPressed, page, [=] { navigate(location->text()); });
    QObject::connect(back, &QPushButton::clicked, page, [=] { if (state->index > 0) navigate(state->history[--state->index], false); });
    QObject::connect(forward, &QPushButton::clicked, page, [=] { if (state->index + 1 < state->history.size()) navigate(state->history[++state->index], false); });
    QObject::connect(up, &QPushButton::clicked, page, [=] { QDir dir(location->text()); dir.cdUp(); navigate(dir.path()); });
    QObject::connect(places, &QListWidget::itemClicked, page, [=](QListWidgetItem* item) { navigate(item->data(Qt::UserRole).toString()); });
    QObject::connect(mode, &QPushButton::toggled, stack, [=](bool checked) { stack->setCurrentIndex(checked ? 1 : 0); mode->setIcon(fileIcon(checked ? FileIcon::Grid : FileIcon::List)); });
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
    QObject::connect(newFile, &QPushButton::clicked, page, [=] { bool ok = false; const auto name = QInputDialog::getText(page, translate("New file"), translate("File name"), QLineEdit::Normal, "", &ok); if (!ok) return; if (!validFileName(name)) { status->setText(translate("Could not create file. Check its name and permissions.")); return; } QFile file(QDir(location->text()).filePath(name)); if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) status->setText(translate("Could not create file. Check its name and permissions.")); else { file.close(); status->setText(translate("File created.")); } });
    QObject::connect(newFolder, &QPushButton::clicked, page, [=] { bool ok = false; const auto name = QInputDialog::getText(page, translate("New folder"), translate("Folder name"), QLineEdit::Normal, "", &ok); if (!ok) return; if (!validFileName(name) || !QDir(location->text()).mkdir(name)) status->setText(translate("Could not create folder. Check its name and permissions.")); });
    QObject::connect(terminal, &QPushButton::clicked, page, [=] { QString error; auto command = defaultApplicationCommand("terminal", &error); if (!error.isEmpty() || command.isEmpty()) { status->setText(error.isEmpty() ? translate("No default terminal is configured.") : error); return; } const auto program = command.takeFirst(); QProcessEnvironment environment = QProcessEnvironment::systemEnvironment(); environment.insert("PWD", location->text()); auto* process = new QProcess(page); process->setProcessEnvironment(environment); process->setWorkingDirectory(location->text()); process->setProgram(program); process->setArguments(command); process->startDetached(); process->deleteLater(); });
    QObject::connect(rename, &QPushButton::clicked, page, [=] { const auto paths = selection(); if (paths.size() != 1) { status->setText(translate("Select one item to rename.")); return; } const QFileInfo info(paths.first()); bool ok = false; const auto name = QInputDialog::getText(page, translate("Rename"), translate("New name"), QLineEdit::Normal, info.fileName(), &ok); if (!ok || name == info.fileName()) return; if (!validFileName(name) || !QDir().rename(paths.first(), info.dir().filePath(name))) status->setText(translate("Could not rename item. Nothing was overwritten.")); });
    auto shortcut = [page](const QKeySequence& sequence, const std::function<void()>& action) { auto* key = new QAction(page); key->setShortcut(sequence); key->setShortcutContext(Qt::WidgetWithChildrenShortcut); page->addAction(key); QObject::connect(key, &QAction::triggered, page, action); };
    shortcut(QKeySequence("Alt+Left"), [back] { back->click(); }); shortcut(QKeySequence("Alt+Right"), [forward] { forward->click(); }); shortcut(QKeySequence("Alt+Up"), [up] { up->click(); }); shortcut(QKeySequence("Ctrl+L"), [location] { location->setFocus(); location->selectAll(); }); shortcut(QKeySequence("Ctrl+Shift+N"), [newFolder] { newFolder->click(); }); shortcut(QKeySequence("Ctrl+N"), [newFile] { newFile->click(); }); shortcut(QKeySequence("F2"), [rename] { rename->click(); });
    return page;
}
}
