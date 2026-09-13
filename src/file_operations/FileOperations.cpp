#include <LuDash/file_operations/FileOperations.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
namespace LuDash {
bool validFileName(const QString& name) { return !name.isEmpty() && name != "." && name != ".." && !name.contains('/') && !name.contains(QChar(0)) && name.toUtf8().size() <= 255; }
QString performFileOperation(const QString& operation, const QStringList& paths, const QString& destination) {
    if (operation != "copy" && operation != "move" && operation != "trash") return "Unknown file operation.";
    if (paths.isEmpty() || paths.size() > 256) return "Select between 1 and 256 items.";
    if (operation != "trash" && !QFileInfo(destination).isDir()) return "Destination folder does not exist.";
    for (const auto& path : paths) {
        const QFileInfo source(path);
        if (!source.exists() && !source.isSymLink()) return "Source no longer exists: " + path;
        const auto target = QDir(destination).filePath(source.fileName());
        if (operation != "trash" && (QFileInfo::exists(target) || QFileInfo(target).isSymLink())) return "An item already exists; nothing will be overwritten: " + target;
        if (operation == "copy" && (source.isDir() || source.isSymLink())) return "Copy supports regular files. Folder and symbolic-link copying is not implemented.";
    }
    for (const auto& path : paths) {
        const auto target = QDir(destination).filePath(QFileInfo(path).fileName());
        bool success = false;
        if (operation == "copy") success = QFile::copy(path, target);
        else if (operation == "move") success = QDir().rename(path, target);
        else success = QFile::moveToTrash(path);
        if (!success) return "Operation stopped at: " + path + ". Earlier items may have completed. Cross-filesystem moves may require copy and trash.";
    }
    return {};
}
}
