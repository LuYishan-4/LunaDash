#pragma once
#include <QColor>
#include <QIcon>
namespace LunaDash {
enum class FileIcon {
  Home,
  Folder,
  File,
  Desktop,
  Download,
  Picture,
  Music,
  Video,
  Drive,
  Back,
  Forward,
  Up,
  Grid,
  List,
  Plus,
  Copy,
  Cut,
  Paste,
  Rename,
  Trash,
  Search,
  Eye,
  Terminal
};
QIcon fileIcon(FileIcon icon, const QColor &color = QColor("#aebccc"));
} // namespace LunaDash
