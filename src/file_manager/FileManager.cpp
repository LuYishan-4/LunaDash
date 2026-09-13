#include <LuDash/localization/Localization.h>
#include <LuDash/file_manager/FileManager.h>
#include <QtWidgets>

namespace LuDash {
QWidget* createFileManager() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 12);
    auto* tools = new QHBoxLayout;
    auto* up = new QPushButton(LuDash::translate("Up"));
    auto* home = new QPushButton(LuDash::translate("Home"));
    auto* location = new QLineEdit(QDir::homePath());
    location->setObjectName("fileLocation");
    tools->addWidget(up); tools->addWidget(home); tools->addWidget(location, 1);
    layout->addLayout(tools);
    auto* model = new QFileSystemModel(page);
    model->setRootPath(QDir::homePath());
    model->setReadOnly(true);
    auto* view = new QTreeView;
    view->setObjectName("fileView");
    view->setModel(model);
    view->setRootIndex(model->index(QDir::homePath()));
    view->setRootIsDecorated(false);
    view->setAlternatingRowColors(true);
    view->setSortingEnabled(true);
    view->sortByColumn(0, Qt::AscendingOrder);
    view->setColumnWidth(0, 270);
    layout->addWidget(view, 1);
    auto* status = new QLabel(LuDash::translate("Double-click folders to browse, or files to open with the default application."));
    status->setObjectName("muted");
    layout->addWidget(status);
    auto navigate = [=](const QString& path) {
        const QFileInfo info(path);
        if (!info.isDir()) { status->setText(LuDash::translate("Folder not found: ") + path); return; }
        location->setText(info.absoluteFilePath());
        view->setRootIndex(model->index(info.absoluteFilePath()));
        status->setText(info.absoluteFilePath());
    };
    QObject::connect(location, &QLineEdit::returnPressed, page, [=] { navigate(location->text()); });
    QObject::connect(home, &QPushButton::clicked, page, [=] { navigate(QDir::homePath()); });
    QObject::connect(up, &QPushButton::clicked, page, [=] { QDir dir(location->text()); dir.cdUp(); navigate(dir.path()); });
    QObject::connect(view, &QTreeView::doubleClicked, page, [=](const QModelIndex& index) {
        const auto info = model->fileInfo(index);
        if (info.isDir()) navigate(info.absoluteFilePath());
        else if (!QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()))) status->setText(LuDash::translate("Unable to open this file."));
    });
    return page;
}
}
