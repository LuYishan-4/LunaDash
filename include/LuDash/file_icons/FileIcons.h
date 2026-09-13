#pragma once
#include <QIcon>
#include <QColor>
namespace LuDash {
enum class FileIcon { Home, Folder, File, Desktop, Download, Picture, Music, Video, Drive, Back, Forward, Up, Grid, List, Plus, Copy, Cut, Paste, Rename, Trash, Search, Eye };
QIcon fileIcon(FileIcon icon, const QColor& color = QColor("#aebccc"));
}
