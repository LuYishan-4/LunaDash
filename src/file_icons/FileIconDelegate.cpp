#include <LuDash/file_icons/FileIconDelegate.h>
#include <LuDash/file_icons/FileIcons.h>
#include <QFileSystemModel>
#include <QStyleOptionViewItem>
#include <QAbstractItemView>
namespace LuDash {
FileIconDelegate::FileIconDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
void FileIconDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    const auto* model = qobject_cast<const QFileSystemModel*>(index.model());
    if (!model || index.column() != 0) return;
    const auto info = model->fileInfo(index); const auto extension = info.suffix().toLower();
    FileIcon icon = info.isDir() ? FileIcon::Folder : FileIcon::File;
    if (!info.isDir() && QStringList{"png", "jpg", "jpeg", "webp", "svg"}.contains(extension)) icon = FileIcon::Picture;
    else if (!info.isDir() && QStringList{"mp3", "flac", "ogg", "wav"}.contains(extension)) icon = FileIcon::Music;
    else if (!info.isDir() && QStringList{"mp4", "mkv", "webm"}.contains(extension)) icon = FileIcon::Video;
    option->features |= QStyleOptionViewItem::HasDecoration;
    const auto* view = qobject_cast<const QAbstractItemView*>(parent());
    option->decorationSize = view ? view->iconSize() : QSize(24, 24);
    if (option->decorationSize.isEmpty()) option->decorationSize = QSize(24, 24);
    const auto accent = view ? view->window()->property("ludashAccent").value<QColor>() : QColor();
    option->icon = fileIcon(icon, info.isDir() ? (accent.isValid() ? accent : QColor("#9ccbfb")) : QColor("#aebccc"));
}
}
