#include <LuDash/file_manager/FileManager.h>
#include <LuDash/file_manager_actions/FileManagerActions.h>
#include <LuDash/file_icons/FileIcons.h>
#include <LuDash/file_icons/FileIconDelegate.h>
#include <LuDash/localization/Localization.h>
#include <QtWidgets>

namespace LuDash {
QWidget* createFileManager() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 14);
    layout->setSpacing(14);
    auto button = [page](FileIcon icon, const QString& label, const char* name) {
        auto* control = new QPushButton(fileIcon(icon), {}, page);
        control->setObjectName(name);
        control->setProperty("fileTool", true);
        control->setToolTip(label);
        control->setAccessibleName(label);
        control->setIconSize({20, 20});
        control->setFixedSize(36, 36);
        control->setCursor(Qt::PointingHandCursor);
        return control;
    };
    auto* header = new QHBoxLayout;
    auto* brand = new QLabel(page);
    brand->setPixmap(fileIcon(FileIcon::Folder).pixmap(26, 26));
    auto* title = new QLabel(translate("Files"), page);
    title->setObjectName("fileBrand");
    header->addWidget(brand); header->addWidget(title); header->addStretch();
    auto* associations = new QPushButton(translate("File associations"), page);
    associations->setObjectName("fileAssociations");
    header->addWidget(associations);
    auto* search = new QLineEdit(page);
    search->setObjectName("fileSearch");
    search->setPlaceholderText(translate("Search this folder"));
    search->setMaximumWidth(230); search->setMinimumHeight(36);
    search->addAction(fileIcon(FileIcon::Search), QLineEdit::LeadingPosition);
    header->addWidget(search); layout->addLayout(header);
    auto* navigation = new QHBoxLayout;
    auto* back = button(FileIcon::Back, translate("Back"), "fileBack");
    auto* forward = button(FileIcon::Forward, translate("Forward"), "fileForward");
    auto* up = button(FileIcon::Up, translate("Up"), "fileUp");
    auto* location = new QLineEdit(page);
    location->setObjectName("fileLocation"); location->setMinimumHeight(38);
    location->setAccessibleName(translate("Location"));
    location->addAction(fileIcon(FileIcon::Folder), QLineEdit::LeadingPosition);
    auto* hidden = button(FileIcon::Eye, translate("Show hidden files"), "fileHidden");
    hidden->setCheckable(true);
    auto* mode = button(FileIcon::Grid, translate("Switch view"), "fileMode");
    mode->setCheckable(true); mode->setChecked(true);
    navigation->addWidget(back); navigation->addWidget(forward); navigation->addWidget(up);
    navigation->addWidget(location, 1); navigation->addWidget(hidden); navigation->addWidget(mode);
    layout->addLayout(navigation);
    auto* splitter = new QSplitter(page); splitter->setHandleWidth(12);
    auto* sidebar = new QWidget(splitter);
    auto* sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(0, 8, 0, 0);
    auto* sideTitle = new QLabel(translate("LIBRARY"), sidebar); sideTitle->setObjectName("fileSection");
    sideLayout->addWidget(sideTitle);
    auto* places = new QListWidget(sidebar); places->setObjectName("filePlaces");
    places->setIconSize({20, 20}); places->setSpacing(4); sideLayout->addWidget(places);
    sidebar->setMinimumWidth(145); sidebar->setMaximumWidth(200);
    auto* browser = new QWidget(splitter);
    auto* browserLayout = new QVBoxLayout(browser); browserLayout->setContentsMargins(0, 0, 0, 0);
    auto* actions = new QHBoxLayout;
    auto* folderTitle = new QLabel(browser); folderTitle->setObjectName("fileFolderTitle");
    folderTitle->setTextFormat(Qt::PlainText);
    actions->addWidget(folderTitle); actions->addStretch();
    auto addButton = [=](FileIcon icon, const QString& label, const char* name) {
        auto* control = button(icon, label, name); actions->addWidget(control);
    };
    addButton(FileIcon::File, translate("New file"), "fileNewFile");
    addButton(FileIcon::Plus, translate("New folder"), "fileNewFolder");
    addButton(FileIcon::Terminal, translate("Open terminal here"), "fileTerminal");
    addButton(FileIcon::Copy, translate("Copy"), "fileCopy");
    addButton(FileIcon::Cut, translate("Cut"), "fileCut");
    addButton(FileIcon::Paste, translate("Paste"), "filePaste");
    addButton(FileIcon::Rename, translate("Rename"), "fileRename");
    addButton(FileIcon::Trash, translate("Move to Trash"), "fileTrash");
    browserLayout->addLayout(actions);
    auto* view = new QTreeView(browser); view->setObjectName("fileView");
    view->setRootIsDecorated(false); view->setSortingEnabled(true);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setItemDelegate(new FileIconDelegate(view)); view->setIconSize({22, 22});
    view->header()->setStretchLastSection(true);
    auto* icons = new QListView(browser); icons->setObjectName("fileIcons");
    icons->setViewMode(QListView::IconMode); icons->setResizeMode(QListView::Adjust);
    icons->setMovement(QListView::Static); icons->setItemDelegate(new FileIconDelegate(icons));
    icons->setIconSize({52, 52}); icons->setGridSize({140, 116}); icons->setSpacing(8);
    icons->setWordWrap(true); icons->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto* stack = new QStackedWidget(browser); stack->setObjectName("fileViewStack");
    stack->addWidget(view); stack->addWidget(icons); stack->setCurrentIndex(1);
    browserLayout->addWidget(stack, 1);
    splitter->addWidget(sidebar); splitter->addWidget(browser);
    splitter->setStretchFactor(1, 1); splitter->setSizes({170, 780}); layout->addWidget(splitter, 1);
    auto* status = new QLabel(page); status->setObjectName("fileStatus");
    status->setTextFormat(Qt::PlainText); status->setWordWrap(true); layout->addWidget(status);
    new FileManagerActions(page);
    return page;
}
} // namespace LuDash
