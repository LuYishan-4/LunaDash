#include <LuDash/localization/Localization.h>
#include <LuDash/file_manager/FileManager.h>
#include <LuDash/file_operations/FileOperations.h>
#include <LuDash/file_icons/FileIcons.h>
#include <LuDash/file_icons/FileIconDelegate.h>
#include <LuDash/default_applications/DefaultApplications.h>
#include <QtWidgets>
#include <QtConcurrent/QtConcurrentRun>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <memory>

namespace LuDash {
namespace {
struct DesktopApplication {
    QString name;
    QString desktopFile;
    QString icon;
};

enum class FileOpenMode { Normal, OpenWith, SetDefault };

QString associationKey(const QFileInfo &info) {
    const QString extension = info.completeSuffix().trimmed().toLower();
    if (!extension.isEmpty())
        return QStringLiteral("ext_") + extension;
    QString mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchDefault).name();
    mime.replace('/', '_');
    return QStringLiteral("mime_") + mime;
}

QString associationLabel(const QFileInfo &info) {
    const QString extension = info.completeSuffix().trimmed().toLower();
    if (!extension.isEmpty())
        return QStringLiteral(".") + extension;
    return QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchDefault).name();
}

QString storedAssociation(const QFileInfo &info) {
    return QSettings().value(QStringLiteral("fileAssociations/") + associationKey(info)).toString();
}

bool saveAssociation(const QFileInfo &info, const QString &desktopFile, QString *error) {
    QSettings settings;
    settings.setValue(QStringLiteral("fileAssociations/") + associationKey(info), desktopFile);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error)
            *error = translate("Could not save the file association.");
        return false;
    }

    if (desktopFile != QStringLiteral("__system__") && !desktopFile.isEmpty()) {
        const QString xdgMime = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
        const QString mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchDefault).name();
        if (!xdgMime.isEmpty() && !mime.isEmpty())
            QProcess::startDetached(xdgMime, {QStringLiteral("default"), QFileInfo(desktopFile).fileName(), mime});
    }
    return true;
}

void clearAssociation(const QFileInfo &info) {
    QSettings settings;
    settings.remove(QStringLiteral("fileAssociations/") + associationKey(info));
    settings.sync();
}

QStringList applicationDirectories() {
    QStringList directories = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    directories << QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications")
                << QStringLiteral("/var/lib/flatpak/exports/share/applications")
                << QStringLiteral("/usr/local/share/applications")
                << QStringLiteral("/usr/share/applications");
    directories.removeDuplicates();
    return directories;
}

QStringList desktopMimeTypes(QSettings &desktop) {
    const QVariant value = desktop.value(QStringLiteral("MimeType"));
    QStringList types = value.toStringList();
    if (types.size() <= 1)
        types = value.toString().split(';', Qt::SkipEmptyParts);
    for (QString &type : types)
        type = type.trimmed();
    return types;
}

QList<DesktopApplication> applicationsForFile(const QFileInfo &info) {
    const QString mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchDefault).name();
    QList<DesktopApplication> applications;
    QSet<QString> seen;

    for (const QString &directory : applicationDirectories()) {
        QDir dir(directory);
        if (!dir.exists())
            continue;
        QDirIterator iterator(directory, {QStringLiteral("*.desktop")}, QDir::Files);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            const QString id = QFileInfo(path).fileName();
            if (seen.contains(id))
                continue;

            QSettings desktop(path, QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));
            if (desktop.value(QStringLiteral("Type")).toString() != QStringLiteral("Application") ||
                desktop.value(QStringLiteral("Hidden")).toBool() ||
                desktop.value(QStringLiteral("NoDisplay")).toBool() ||
                desktop.value(QStringLiteral("Exec")).toString().trimmed().isEmpty() ||
                !desktopMimeTypes(desktop).contains(mime)) {
                desktop.endGroup();
                continue;
            }

            QString name = desktop.value(QStringLiteral("Name[") + QLocale().name() + QStringLiteral("]")).toString();
            if (name.isEmpty())
                name = desktop.value(QStringLiteral("Name[") + QLocale().name().section('_', 0, 0) + QStringLiteral("]")).toString();
            if (name.isEmpty())
                name = desktop.value(QStringLiteral("Name")).toString();
            const QString icon = desktop.value(QStringLiteral("Icon")).toString();
            desktop.endGroup();

            if (name.isEmpty())
                name = QFileInfo(path).completeBaseName();
            seen.insert(id);
            applications.push_back({name, path, icon});
        }
    }

    std::sort(applications.begin(), applications.end(), [](const DesktopApplication &left, const DesktopApplication &right) {
        return QString::localeAwareCompare(left.name, right.name) < 0;
    });
    return applications;
}

bool launchDesktopApplication(const QString &desktopFile, const QString &filePath, QString *error) {
    if (desktopFile.isEmpty() || desktopFile == QStringLiteral("__system__")) {
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath)))
            return true;
        if (error)
            *error = translate("Unable to open this file.");
        return false;
    }

    if (!QFileInfo::exists(desktopFile)) {
        if (error)
            *error = translate("The selected application is no longer installed.");
        return false;
    }

    const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (!gio.isEmpty() && QProcess::startDetached(gio, {QStringLiteral("launch"), desktopFile, filePath}))
        return true;

    QSettings desktop(desktopFile, QSettings::IniFormat);
    desktop.beginGroup(QStringLiteral("Desktop Entry"));
    const QString exec = desktop.value(QStringLiteral("Exec")).toString();
    const QString applicationName = desktop.value(QStringLiteral("Name")).toString();
    desktop.endGroup();
    QStringList command = QProcess::splitCommand(exec);
    if (command.isEmpty()) {
        if (error)
            *error = translate("The selected application has no launch command.");
        return false;
    }

    QString program = command.takeFirst();
    QStringList arguments;
    bool insertedFile = false;
    for (QString token : command) {
        if (token == QStringLiteral("%f") || token == QStringLiteral("%F") ||
            token == QStringLiteral("%u") || token == QStringLiteral("%U")) {
            arguments << filePath;
            insertedFile = true;
            continue;
        }
        if (token == QStringLiteral("%i"))
            continue;
        token.replace(QStringLiteral("%c"), applicationName);
        token.replace(QStringLiteral("%k"), desktopFile);
        if (token.contains(QStringLiteral("%f")) || token.contains(QStringLiteral("%F")) ||
            token.contains(QStringLiteral("%u")) || token.contains(QStringLiteral("%U"))) {
            token.replace(QStringLiteral("%f"), filePath);
            token.replace(QStringLiteral("%F"), filePath);
            token.replace(QStringLiteral("%u"), filePath);
            token.replace(QStringLiteral("%U"), filePath);
            insertedFile = true;
        }
        token.remove(QRegularExpression(QStringLiteral("%[dDnNickvm]")));
        if (!token.isEmpty())
            arguments << token;
    }
    if (!insertedFile)
        arguments << filePath;

    if (QProcess::startDetached(program, arguments))
        return true;
    if (error)
        *error = translate("Unable to start the selected application.");
    return false;
}

bool chooseApplication(QWidget *parent, const QFileInfo &info, bool defaultChecked,
                       QString *desktopFile, bool *remember) {
    QDialog dialog(parent);
    dialog.setWindowTitle(translate("Open with"));
    dialog.setMinimumWidth(440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *message = new QLabel(QString(translate("Choose an application for %1 files.")).arg(associationLabel(info)), &dialog);
    message->setWordWrap(true);
    layout->addWidget(message);

    auto *list = new QListWidget(&dialog);
    list->setIconSize({28, 28});
    auto *system = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("system-run")), translate("System default"), list);
    system->setData(Qt::UserRole, QStringLiteral("__system__"));
    for (const auto &application : applicationsForFile(info)) {
        QIcon icon = QIcon::fromTheme(application.icon);
        auto *item = new QListWidgetItem(icon, application.name, list);
        item->setData(Qt::UserRole, application.desktopFile);
        item->setToolTip(application.desktopFile);
    }
    list->setCurrentRow(0);
    const QString current = storedAssociation(info);
    if (!current.isEmpty()) {
        for (int row = 0; row < list->count(); ++row) {
            if (list->item(row)->data(Qt::UserRole).toString() == current) {
                list->setCurrentRow(row);
                break;
            }
        }
    }
    layout->addWidget(list, 1);

    auto *always = new QCheckBox(QString(translate("Always use this application for %1 files")).arg(associationLabel(info)), &dialog);
    always->setChecked(defaultChecked);
    layout->addWidget(always);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem *) { dialog.accept(); });
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted || !list->currentItem())
        return false;
    if (desktopFile)
        *desktopFile = list->currentItem()->data(Qt::UserRole).toString();
    if (remember)
        *remember = always->isChecked();
    return true;
}

bool openFile(QWidget *parent, const QFileInfo &info, FileOpenMode mode, QString *error) {
    QString selected;
    bool remember = mode != FileOpenMode::OpenWith;

    if (mode == FileOpenMode::Normal) {
        selected = storedAssociation(info);
        if (!selected.isEmpty()) {
            if (launchDesktopApplication(selected, info.absoluteFilePath(), error))
                return true;
            clearAssociation(info);
        }
    }

    if (!chooseApplication(parent, info, mode != FileOpenMode::OpenWith, &selected, &remember))
        return false;

    if (mode == FileOpenMode::SetDefault)
        remember = true;
    if (remember && !saveAssociation(info, selected, error))
        return false;
    if (mode == FileOpenMode::SetDefault)
        return true;
    return launchDesktopApplication(selected, info.absoluteFilePath(), error);
}

QString fileProperties(const QFileInfo &info) {
    const QString mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchDefault).comment();
    const QString size = info.isDir() ? QStringLiteral("—") : QLocale().formattedDataSize(info.size());
    const QString modified = QLocale().toString(info.lastModified(), QLocale::ShortFormat);
    return QString(translate("Name: %1\nType: %2\nSize: %3\nModified: %4\nPath: %5"))
        .arg(info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName(),
             mime.isEmpty() ? (info.isDir() ? translate("Folder") : translate("File")) : mime,
             size, modified, info.absoluteFilePath());
}
} // namespace

QWidget* createFileManager() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 14);
    layout->setSpacing(18);

    auto button = [page](FileIcon icon, const char* label, const char* name) {
        auto* control = new QPushButton(fileIcon(icon), "", page);
        control->setObjectName(name);
        control->setProperty("fileTool", true);
        control->setToolTip(translate(label));
        control->setAccessibleName(translate(label));
        control->setIconSize({20, 20});
        control->setFixedSize(36, 36);
        control->setCursor(Qt::PointingHandCursor);
        return control;
    };

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    auto* brand = new QLabel;
    brand->setPixmap(fileIcon(FileIcon::Folder).pixmap(26, 26));
    auto* title = new QLabel(translate("Files"));
    title->setObjectName("fileBrand");
    header->addWidget(brand);
    header->addWidget(title);
    header->addStretch();
    auto* search = new QLineEdit;
    search->setObjectName("fileSearch");
    search->setPlaceholderText(translate("Search this folder"));
    search->setFixedWidth(230);
    search->setMinimumHeight(36);
    search->addAction(fileIcon(FileIcon::Search), QLineEdit::LeadingPosition);
    header->addWidget(search);
    layout->addLayout(header);

    auto* navigation = new QHBoxLayout;
    navigation->setSpacing(6);
    auto* back = button(FileIcon::Back, "Back", "fileBack");
    auto* forward = button(FileIcon::Forward, "Forward", "fileForward");
    auto* up = button(FileIcon::Up, "Up", "fileUp");
    auto* location = new QLineEdit(QDir::homePath());
    location->setObjectName("fileLocation");
    location->setMinimumHeight(38);
    location->addAction(fileIcon(FileIcon::Folder), QLineEdit::LeadingPosition);
    auto* hidden = button(FileIcon::Eye, "Show hidden files", "fileHidden");
    hidden->setCheckable(true);
    auto* mode = button(FileIcon::Grid, "Switch view", "fileMode");
    mode->setCheckable(true);
    mode->setChecked(true);
    navigation->addWidget(back);
    navigation->addWidget(forward);
    navigation->addWidget(up);
    navigation->addSpacing(8);
    navigation->addWidget(location, 1);
    navigation->addSpacing(8);
    navigation->addWidget(hidden);
    navigation->addWidget(mode);
    layout->addLayout(navigation);

    auto* splitter = new QSplitter;
    splitter->setHandleWidth(22);
    auto* sidebar = new QWidget;
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 8, 0, 0);
    sideLayout->setSpacing(12);
    auto* sideTitle = new QLabel(translate("LIBRARY"));
    sideTitle->setObjectName("fileSection");
    sideTitle->setContentsMargins(14, 0, 0, 0);
    sideLayout->addWidget(sideTitle);
    auto* places = new QListWidget;
    places->setObjectName("filePlaces");
    places->setIconSize({20, 20});
    places->setSpacing(4);
    places->setContextMenuPolicy(Qt::CustomContextMenu);
    sideLayout->addWidget(places);
    sidebar->setMinimumWidth(145);
    sidebar->setMaximumWidth(200);

    auto addPlace = [places](const char* label, const QString& path, FileIcon icon) {
        if (path.isEmpty()) return;
        auto* item = new QListWidgetItem(fileIcon(icon), translate(label), places);
        item->setData(Qt::UserRole, path);
    };
    addPlace("Home", QDir::homePath(), FileIcon::Home);
    addPlace("Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), FileIcon::Desktop);
    addPlace("Documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), FileIcon::File);
    addPlace("Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), FileIcon::Download);
    addPlace("Pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), FileIcon::Picture);
    addPlace("Music", QStandardPaths::writableLocation(QStandardPaths::MusicLocation), FileIcon::Music);
    addPlace("Videos", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), FileIcon::Video);
    addPlace("File system", "/", FileIcon::Drive);
    for (const auto& volume : QStorageInfo::mountedVolumes()) {
        if (volume.isValid() && volume.isReady() && volume.rootPath() != "/" &&
            (volume.rootPath().startsWith("/run/media/") || volume.rootPath().startsWith("/media/") || volume.rootPath().startsWith("/mnt/"))) {
            auto* item = new QListWidgetItem(fileIcon(FileIcon::Drive), volume.displayName(), places);
            item->setData(Qt::UserRole, volume.rootPath());
        }
    }

    auto* browser = new QWidget;
    auto* browserLayout = new QVBoxLayout(browser);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(14);
    auto* actions = new QHBoxLayout;
    actions->setSpacing(5);
    auto* folderTitle = new QLabel(translate("Home"));
    folderTitle->setObjectName("fileFolderTitle");
    actions->addWidget(folderTitle);
    actions->addStretch();
    auto* newFile = new QPushButton(fileIcon(FileIcon::File), translate("New file"));
    newFile->setObjectName("fileNewFile");
    newFile->setIconSize({17, 17});
    actions->addWidget(newFile);
    auto* newFolder = new QPushButton(fileIcon(FileIcon::Plus), translate("New folder"));
    newFolder->setObjectName("fileNewFolder");
    newFolder->setIconSize({17, 17});
    actions->addWidget(newFolder);
    auto* terminal = new QPushButton(fileIcon(FileIcon::Terminal), translate("Open terminal here"));
    terminal->setObjectName("fileTerminal");
    terminal->setIconSize({17, 17});
    actions->addWidget(terminal);
    actions->addSpacing(8);
    auto addButton = [actions, button](FileIcon icon, const char* label, const char* name) {
        auto* control = button(icon, label, name);
        actions->addWidget(control);
        return control;
    };
    auto* copy = addButton(FileIcon::Copy, "Copy", "fileCopy");
    auto* cut = addButton(FileIcon::Cut, "Cut", "fileCut");
    auto* paste = addButton(FileIcon::Paste, "Paste", "filePaste");
    auto* rename = addButton(FileIcon::Rename, "Rename", "fileRename");
    auto* trash = addButton(FileIcon::Trash, "Move to Trash", "fileTrash");
    browserLayout->addLayout(actions);

    auto* model = new QFileSystemModel(page);
    model->setRootPath(QDir::homePath());
    model->setReadOnly(true);
    auto* view = new QTreeView;
    view->setObjectName("fileView");
    view->setModel(model);
    view->setRootIsDecorated(false);
    view->setSortingEnabled(true);
    view->sortByColumn(0, Qt::AscendingOrder);
    view->setColumnWidth(0, 290);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setAlternatingRowColors(false);
    view->setItemDelegate(new FileIconDelegate(view));
    view->setIconSize({22, 22});
    view->header()->setStretchLastSection(true);
    view->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* icons = new QListView;
    icons->setObjectName("fileIcons");
    icons->setModel(model);
    icons->setViewMode(QListView::IconMode);
    icons->setResizeMode(QListView::Adjust);
    icons->setMovement(QListView::Static);
    icons->setItemDelegate(new FileIconDelegate(icons));
    icons->setIconSize({52, 52});
    icons->setGridSize({140, 116});
    icons->setSpacing(8);
    icons->setWordWrap(true);
    icons->setSelectionMode(QAbstractItemView::ExtendedSelection);
    icons->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* stack = new QStackedWidget;
    stack->addWidget(view);
    stack->addWidget(icons);
    stack->setCurrentIndex(1);
    browserLayout->addWidget(stack, 1);
    splitter->addWidget(sidebar);
    splitter->addWidget(browser);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({170, 780});
    layout->addWidget(splitter, 1);

    auto* status = new QLabel;
    status->setObjectName("fileStatus");
    status->setWordWrap(true);
    layout->addWidget(status);

    struct Navigation { QStringList history; int index = -1; QStringList clipboard; bool cut = false; };
    auto state = std::make_shared<Navigation>();

    auto navigate = [=](const QString& path, bool remember = true) {
        const QFileInfo info(path);
        if (!info.isDir()) {
            status->setText(translate("Folder not found: ") + path);
            return;
        }
        const auto absolute = info.absoluteFilePath();
        if (remember && (state->index < 0 || state->history.value(state->index) != absolute)) {
            state->history = state->history.mid(0, state->index + 1);
            state->history << absolute;
            state->index++;
        }
        model->sort(0, Qt::AscendingOrder);
        location->setText(absolute);
        const auto index = model->index(absolute);
        view->setRootIndex(index);
        icons->setRootIndex(index);
        back->setEnabled(state->index > 0);
        forward->setEnabled(state->index + 1 < state->history.size());
        folderTitle->setText(absolute == QDir::homePath() ? translate("Home") : info.fileName().isEmpty() ? translate("File system") : info.fileName());
        for (int row = 0; row < places->count(); ++row)
            places->item(row)->setSelected(places->item(row)->data(Qt::UserRole).toString() == absolute);
        status->setText(QString(translate("%1 items")).arg(model->rowCount(index)));
    };

    auto launchTerminal = [=](const QString &directory) {
        QString error;
        auto command = defaultApplicationCommand("terminal", &error);
        if (!error.isEmpty() || command.isEmpty()) {
            status->setText(error.isEmpty() ? translate("No default terminal is configured.") : error);
            return;
        }
        const auto program = command.takeFirst();
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PWD", directory);
        QProcess::startDetached(program, command, directory);
    };

    auto selectedPaths = [=] {
        QStringList paths;
        auto* active = mode->isChecked() ? static_cast<QAbstractItemView*>(icons) : static_cast<QAbstractItemView*>(view);
        for (const auto& index : active->selectionModel()->selectedIndexes())
            if (index.column() == 0)
                paths << model->filePath(index);
        paths.removeDuplicates();
        return paths;
    };

    auto openIndex = [=](const QModelIndex& index) {
        if (!index.isValid())
            return;
        const auto info = model->fileInfo(index.sibling(index.row(), 0));
        if (info.isDir()) {
            navigate(info.absoluteFilePath());
            return;
        }
        if (info.isExecutable()) {
            status->setText(translate("Executable files are not launched from Files. Use the terminal to run trusted programs."));
            return;
        }
        QString error;
        if (!openFile(page, info, FileOpenMode::Normal, &error) && !error.isEmpty())
            status->setText(error);
    };

    QObject::connect(model, &QFileSystemModel::directoryLoaded, page, [=](const QString& path) {
        if (path == location->text())
            status->setText(QString(translate("%1 items")).arg(model->rowCount(view->rootIndex())));
    });
    navigate(QDir::homePath());
    QObject::connect(location, &QLineEdit::returnPressed, page, [=] { navigate(location->text()); });
    QObject::connect(back, &QPushButton::clicked, page, [=] { if (state->index > 0) navigate(state->history[--state->index], false); });
    QObject::connect(forward, &QPushButton::clicked, page, [=] { if (state->index + 1 < state->history.size()) navigate(state->history[++state->index], false); });
    QObject::connect(up, &QPushButton::clicked, page, [=] { QDir dir(location->text()); dir.cdUp(); navigate(dir.path()); });
    QObject::connect(places, &QListWidget::itemClicked, page, [=](QListWidgetItem* item) { navigate(item->data(Qt::UserRole).toString()); });
    QObject::connect(mode, &QPushButton::toggled, stack, [=](bool checked) { stack->setCurrentIndex(checked ? 1 : 0); mode->setIcon(fileIcon(checked ? FileIcon::Grid : FileIcon::List)); });
    QObject::connect(hidden, &QPushButton::toggled, model, [=](bool checked) { model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | (checked ? QDir::Hidden : QDir::Filters{})); });
    QObject::connect(search, &QLineEdit::textChanged, model, [=](const QString& value) { model->setNameFilters(value.isEmpty() ? QStringList{} : QStringList{"*" + value + "*"}); model->setNameFilterDisables(false); });
    QObject::connect(view, &QTreeView::doubleClicked, page, openIndex);
    QObject::connect(icons, &QListView::doubleClicked, page, openIndex);

    auto* watcher = new QFutureWatcher<QString>(page);
    auto run = [=](const QString& operation, const QStringList& paths) {
        if (watcher->isRunning() || paths.isEmpty()) return;
        status->setText(translate("Working..."));
        paste->setEnabled(false);
        trash->setEnabled(false);
        watcher->setFuture(QtConcurrent::run(performFileOperation, operation, paths, location->text()));
    };
    QObject::connect(watcher, &QFutureWatcher<QString>::finished, page, [=] {
        status->setText(watcher->result().isEmpty() ? translate("Operation completed.") : watcher->result());
        paste->setEnabled(true);
        trash->setEnabled(true);
    });

    QObject::connect(copy, &QPushButton::clicked, page, [=] { state->clipboard = selectedPaths(); state->cut = false; status->setText(translate("Selection copied. Choose a folder and Paste.")); });
    QObject::connect(cut, &QPushButton::clicked, page, [=] { state->clipboard = selectedPaths(); state->cut = true; status->setText(translate("Selection cut. Choose a folder and Paste.")); });
    QObject::connect(paste, &QPushButton::clicked, page, [=] { run(state->cut ? "move" : "copy", state->clipboard); });
    QObject::connect(trash, &QPushButton::clicked, page, [=] {
        const auto paths = selectedPaths();
        if (!paths.isEmpty() && QMessageBox::question(page, translate("Move to Trash"), QString(translate("Move %1 selected items to Trash?")).arg(paths.size())) == QMessageBox::Yes)
            run("trash", paths);
    });
    QObject::connect(newFile, &QPushButton::clicked, page, [=] {
        bool ok = false;
        const auto name = QInputDialog::getText(page, translate("New file"), translate("File name"), QLineEdit::Normal, "", &ok);
        if (!ok) return;
        if (!validFileName(name)) { status->setText(translate("Could not create file. Check its name and permissions.")); return; }
        QFile file(QDir(location->text()).filePath(name));
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            status->setText(translate("Could not create file. Check its name and permissions."));
        else { file.close(); status->setText(translate("File created.")); }
    });
    QObject::connect(newFolder, &QPushButton::clicked, page, [=] {
        bool ok = false;
        const auto name = QInputDialog::getText(page, translate("New folder"), translate("Folder name"), QLineEdit::Normal, "", &ok);
        if (!ok) return;
        if (!validFileName(name) || !QDir(location->text()).mkdir(name))
            status->setText(translate("Could not create folder. Check its name and permissions."));
    });
    QObject::connect(terminal, &QPushButton::clicked, page, [=] { launchTerminal(location->text()); });
    QObject::connect(rename, &QPushButton::clicked, page, [=] {
        const auto paths = selectedPaths();
        if (paths.size() != 1) { status->setText(translate("Select one item to rename.")); return; }
        const QFileInfo info(paths.first());
        bool ok = false;
        const auto name = QInputDialog::getText(page, translate("Rename"), translate("New name"), QLineEdit::Normal, info.fileName(), &ok);
        if (!ok || name == info.fileName()) return;
        if (!validFileName(name) || !QDir().rename(paths.first(), info.dir().filePath(name)))
            status->setText(translate("Could not rename item. Nothing was overwritten."));
    });

    auto showContextMenu = [=](QAbstractItemView *source, const QPoint &position) {
        QModelIndex index = source->indexAt(position);
        if (index.isValid()) {
            index = index.sibling(index.row(), 0);
            if (!source->selectionModel()->isSelected(index)) {
                source->selectionModel()->clearSelection();
                source->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                source->setCurrentIndex(index);
            }
        }

        const QStringList paths = selectedPaths();
        const bool single = paths.size() == 1;
        const QFileInfo info(single ? paths.first() : QString());
        QMenu menu(page);

        if (single) {
            auto *openAction = menu.addAction(fileIcon(info.isDir() ? FileIcon::Folder : FileIcon::File), translate("Open"));
            QObject::connect(openAction, &QAction::triggered, page, [=] { openIndex(model->index(paths.first())); });

            if (info.isFile() && !info.isExecutable()) {
                auto *openWith = menu.addAction(translate("Open with..."));
                QObject::connect(openWith, &QAction::triggered, page, [=] {
                    QString error;
                    if (!openFile(page, info, FileOpenMode::OpenWith, &error) && !error.isEmpty()) status->setText(error);
                });
                auto *setDefault = menu.addAction(translate("Set default application for this file type..."));
                QObject::connect(setDefault, &QAction::triggered, page, [=] {
                    QString error;
                    if (openFile(page, info, FileOpenMode::SetDefault, &error))
                        status->setText(QString(translate("Default application saved for %1 files.")).arg(associationLabel(info)));
                    else if (!error.isEmpty())
                        status->setText(error);
                });
                if (!storedAssociation(info).isEmpty()) {
                    auto *resetDefault = menu.addAction(translate("Reset default application for this file type"));
                    QObject::connect(resetDefault, &QAction::triggered, page, [=] {
                        clearAssociation(info);
                        status->setText(QString(translate("Default application reset for %1 files.")).arg(associationLabel(info)));
                    });
                }
            }

            if (info.isDir()) {
                auto *terminalHere = menu.addAction(fileIcon(FileIcon::Terminal), translate("Open terminal here"));
                QObject::connect(terminalHere, &QAction::triggered, page, [=] { launchTerminal(info.absoluteFilePath()); });
            }

            menu.addSeparator();
            auto *copyAction = menu.addAction(fileIcon(FileIcon::Copy), translate("Copy"));
            QObject::connect(copyAction, &QAction::triggered, copy, &QPushButton::click);
            auto *cutAction = menu.addAction(fileIcon(FileIcon::Cut), translate("Cut"));
            QObject::connect(cutAction, &QAction::triggered, cut, &QPushButton::click);
            auto *renameAction = menu.addAction(fileIcon(FileIcon::Rename), translate("Rename"));
            renameAction->setEnabled(single);
            QObject::connect(renameAction, &QAction::triggered, rename, &QPushButton::click);
            auto *trashAction = menu.addAction(fileIcon(FileIcon::Trash), translate("Move to Trash"));
            QObject::connect(trashAction, &QAction::triggered, trash, &QPushButton::click);
            menu.addSeparator();
            auto *copyPath = menu.addAction(translate("Copy path"));
            QObject::connect(copyPath, &QAction::triggered, page, [=] { QGuiApplication::clipboard()->setText(paths.join('\n')); });
            auto *properties = menu.addAction(translate("Properties"));
            QObject::connect(properties, &QAction::triggered, page, [=] {
                QMessageBox::information(page, translate("Properties"), fileProperties(info));
            });
        } else {
            auto *newFileAction = menu.addAction(fileIcon(FileIcon::File), translate("New file"));
            QObject::connect(newFileAction, &QAction::triggered, newFile, &QPushButton::click);
            auto *newFolderAction = menu.addAction(fileIcon(FileIcon::Plus), translate("New folder"));
            QObject::connect(newFolderAction, &QAction::triggered, newFolder, &QPushButton::click);
            auto *pasteAction = menu.addAction(fileIcon(FileIcon::Paste), translate("Paste"));
            pasteAction->setEnabled(!state->clipboard.isEmpty());
            QObject::connect(pasteAction, &QAction::triggered, paste, &QPushButton::click);
            auto *terminalHere = menu.addAction(fileIcon(FileIcon::Terminal), translate("Open terminal here"));
            QObject::connect(terminalHere, &QAction::triggered, page, [=] { launchTerminal(location->text()); });
            menu.addSeparator();
            auto *refresh = menu.addAction(translate("Refresh"));
            QObject::connect(refresh, &QAction::triggered, page, [=] { navigate(location->text(), false); });
            auto *hiddenAction = menu.addAction(translate("Show hidden files"));
            hiddenAction->setCheckable(true);
            hiddenAction->setChecked(hidden->isChecked());
            QObject::connect(hiddenAction, &QAction::toggled, hidden, &QPushButton::setChecked);
        }
        menu.exec(source->viewport()->mapToGlobal(position));
    };

    QObject::connect(view, &QTreeView::customContextMenuRequested, page, [=](const QPoint &position) { showContextMenu(view, position); });
    QObject::connect(icons, &QListView::customContextMenuRequested, page, [=](const QPoint &position) { showContextMenu(icons, position); });
    QObject::connect(places, &QListWidget::customContextMenuRequested, page, [=](const QPoint &position) {
        auto *item = places->itemAt(position);
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        QMenu menu(page);
        auto *openAction = menu.addAction(fileIcon(FileIcon::Folder), translate("Open"));
        QObject::connect(openAction, &QAction::triggered, page, [=] { navigate(path); });
        auto *terminalAction = menu.addAction(fileIcon(FileIcon::Terminal), translate("Open terminal here"));
        QObject::connect(terminalAction, &QAction::triggered, page, [=] { launchTerminal(path); });
        auto *copyPath = menu.addAction(translate("Copy path"));
        QObject::connect(copyPath, &QAction::triggered, page, [=] { QGuiApplication::clipboard()->setText(path); });
        menu.exec(places->viewport()->mapToGlobal(position));
    });

    auto shortcut = [page](const QKeySequence& sequence, const std::function<void()>& action) {
        auto* key = new QAction(page);
        key->setShortcut(sequence);
        key->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        page->addAction(key);
        QObject::connect(key, &QAction::triggered, page, action);
    };
    shortcut(QKeySequence("Alt+Left"), [back] { back->click(); });
    shortcut(QKeySequence("Alt+Right"), [forward] { forward->click(); });
    shortcut(QKeySequence("Alt+Up"), [up] { up->click(); });
    shortcut(QKeySequence("Ctrl+L"), [location] { location->setFocus(); location->selectAll(); });
    shortcut(QKeySequence("Ctrl+Shift+N"), [newFolder] { newFolder->click(); });
    shortcut(QKeySequence("Ctrl+N"), [newFile] { newFile->click(); });
    shortcut(QKeySequence("F2"), [rename] { rename->click(); });
    shortcut(QKeySequence("Ctrl+R"), [=] { navigate(location->text(), false); });

    return page;
}
}
