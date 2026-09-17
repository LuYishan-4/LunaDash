#include "desktop/FileManager/FileManager.hpp"
#include "desktop/FileManagerActions/FileManagerActions.hpp"
#include "desktop/FileIcons/FileIcons.hpp"
#include "desktop/FileIconDelegate/FileIconDelegate.hpp"
#include "config/Localization/Localization.hpp"
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
    header->addWidget(brand);
    header->addWidget(title);
    header->addStretch();

    auto* associations = new QPushButton(translate("File associations"), page);
    associations->setObjectName("fileAssociations");
    header->addWidget(associations);

    auto* search = new QLineEdit(page);
    search->setObjectName("fileSearch");
    search->setPlaceholderText(translate("Search this folder"));
    search->setMaximumWidth(230);
    search->setMinimumHeight(36);
    search->addAction(fileIcon(FileIcon::Search), QLineEdit::LeadingPosition);
    header->addWidget(search);
    layout->addLayout(header);

    auto* navigation = new QHBoxLayout;
    auto* back = button(FileIcon::Back, translate("Back"), "fileBack");
    auto* forward = button(FileIcon::Forward, translate("Forward"), "fileForward");
    auto* up = button(FileIcon::Up, translate("Up"), "fileUp");
    auto* location = new QLineEdit(page);
    location->setObjectName("fileLocation");
    location->setMinimumHeight(38);
    location->setAccessibleName(translate("Location"));
    location->addAction(fileIcon(FileIcon::Folder), QLineEdit::LeadingPosition);
    auto* hidden = button(FileIcon::Eye, translate("Show hidden files"), "fileHidden");
    hidden->setCheckable(true);
    auto* mode = button(FileIcon::Grid, translate("Switch view"), "fileMode");
    mode->setCheckable(true);
    mode->setChecked(true);
    navigation->addWidget(back);
    navigation->addWidget(forward);
    navigation->addWidget(up);
    navigation->addWidget(location, 1);
    navigation->addWidget(hidden);
    navigation->addWidget(mode);
    layout->addLayout(navigation);

    auto* splitter = new QSplitter(page);
    splitter->setHandleWidth(12);

    auto* sidebar = new QWidget(splitter);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(0, 8, 0, 0);
    sideLayout->setSpacing(8);
    auto* sideTitle = new QLabel(translate("LIBRARY"), sidebar);
    sideTitle->setObjectName("fileSection");
    sideLayout->addWidget(sideTitle);
    auto* places = new QListWidget(sidebar);
    places->setObjectName("filePlaces");
    places->setIconSize({20, 20});
    places->setSpacing(4);
    sideLayout->addWidget(places);
    sidebar->setMinimumWidth(155);
    sidebar->setMaximumWidth(220);

    auto* browser = new QWidget(splitter);
    auto* browserLayout = new QVBoxLayout(browser);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(10);

    auto* actions = new QHBoxLayout;
    auto* folderTitle = new QLabel(browser);
    folderTitle->setObjectName("fileFolderTitle");
    folderTitle->setTextFormat(Qt::PlainText);
    actions->addWidget(folderTitle);
    actions->addStretch();
    auto addButton = [=](FileIcon icon, const QString& label, const char* name) {
        auto* control = button(icon, label, name);
        actions->addWidget(control);
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

    auto* details = new QTreeView(browser);
    details->setObjectName("fileView");
    details->setRootIsDecorated(false);
    details->setSortingEnabled(true);
    details->setSelectionMode(QAbstractItemView::ExtendedSelection);
    details->setSelectionBehavior(QAbstractItemView::SelectRows);
    details->setItemDelegate(new FileIconDelegate(details));
    details->setIconSize({22, 22});
    details->header()->setStretchLastSection(true);

    auto* icons = new QListView(browser);
    icons->setObjectName("fileIcons");
    icons->setViewMode(QListView::IconMode);
    icons->setResizeMode(QListView::Adjust);
    icons->setMovement(QListView::Static);
    icons->setItemDelegate(new FileIconDelegate(icons));
    icons->setIconSize({64, 64});
    icons->setGridSize({148, 126});
    icons->setSpacing(8);
    icons->setWordWrap(true);
    icons->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* stack = new QStackedWidget(browser);
    stack->setObjectName("fileViewStack");
    stack->addWidget(details);
    stack->addWidget(icons);
    stack->setCurrentIndex(1);

    auto* content = new QSplitter(Qt::Horizontal, browser);
    content->setObjectName("fileContentSplitter");
    content->setHandleWidth(10);
    content->addWidget(stack);

    auto* preview = new QFrame(content);
    preview->setObjectName("filePreviewPanel");
    preview->setMinimumWidth(230);
    preview->setMaximumWidth(360);
    auto* previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setSpacing(10);

    auto* previewEyebrow = new QLabel(translate("PREVIEW"), preview);
    previewEyebrow->setObjectName("filePreviewEyebrow");
    previewLayout->addWidget(previewEyebrow);

    auto* previewImage = new QLabel(preview);
    previewImage->setObjectName("filePreviewImage");
    previewImage->setAlignment(Qt::AlignCenter);
    previewImage->setMinimumHeight(190);
    previewImage->setMaximumHeight(260);
    previewImage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewImage->setText(translate("Hover or select a file to preview it."));
    previewImage->setWordWrap(true);
    previewLayout->addWidget(previewImage, 1);

    auto* previewTitle = new QLabel(preview);
    previewTitle->setObjectName("filePreviewTitle");
    previewTitle->setTextFormat(Qt::PlainText);
    previewTitle->setWordWrap(true);
    previewLayout->addWidget(previewTitle);

    auto* previewMeta = new QLabel(preview);
    previewMeta->setObjectName("filePreviewMeta");
    previewMeta->setTextFormat(Qt::PlainText);
    previewMeta->setWordWrap(true);
    previewMeta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLayout->addWidget(previewMeta);

    auto* previewHint = new QLabel(translate("Images preview directly. Video thumbnails use ffmpeg when available."), preview);
    previewHint->setObjectName("filePreviewHint");
    previewHint->setWordWrap(true);
    previewLayout->addWidget(previewHint);

    content->addWidget(preview);
    content->setStretchFactor(0, 1);
    content->setStretchFactor(1, 0);
    content->setSizes({760, 270});
    browserLayout->addWidget(content, 1);

    splitter->addWidget(sidebar);
    splitter->addWidget(browser);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({180, 920});
    layout->addWidget(splitter, 1);

    auto* status = new QLabel(page);
    status->setObjectName("fileStatus");
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);

    new FileManagerActions(page);
    return page;
}
} // namespace LuDash
