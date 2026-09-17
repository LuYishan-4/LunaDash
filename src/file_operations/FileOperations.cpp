#include <LuDash/file_operations/FileOperations.h>
#include <LuDash/localization/Localization.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QTemporaryDir>
#include <filesystem>
#include <system_error>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/fs.h>

namespace LuDash {
namespace {
namespace fs = std::filesystem;
fs::path nativePath(const QString& path) { return fs::path(QFile::encodeName(path).constData()); }
bool exists(const QString& path) { const QFileInfo info(path); return info.exists() || info.isSymLink(); }
bool inside(const QString& path, const QString& directory) {
    return path == directory || path.startsWith(directory.endsWith('/') ? directory : directory + '/');
}
bool renameNoReplace(const QString& source, const QString& destination, std::error_code& error) {
#ifdef SYS_renameat2
    if (::syscall(SYS_renameat2, AT_FDCWD, QFile::encodeName(source).constData(),
                  AT_FDCWD, QFile::encodeName(destination).constData(), RENAME_NOREPLACE) == 0) return true;
    error = std::error_code(errno, std::generic_category());
#else
    error = std::make_error_code(std::errc::operation_not_supported);
#endif
    return false;
}
bool copyEntry(const fs::path& source, const fs::path& destination, std::error_code& error, int depth = 0) {
    if (depth > 128) { error = std::make_error_code(std::errc::filename_too_long); return false; }
    const auto status = fs::symlink_status(source, error);
    if (error) return false;
    if (fs::is_symlink(status)) {
        fs::copy_symlink(source, destination, error);
        return !error; // Never follow a link, even a dangling or circular one.
    }
    if (fs::is_regular_file(status)) {
        if (!fs::copy_file(source, destination, fs::copy_options::none, error)) return false;
    } else if (fs::is_directory(status)) {
        if (!fs::create_directory(destination, error)) return false;
        fs::directory_iterator iterator(source, error), end;
        while (!error && iterator != end) {
            const auto entry = iterator->path();
            if (!copyEntry(entry, destination / entry.filename(), error, depth + 1)) return false;
            iterator.increment(error);
        }
        if (error) return false;
    } else {
        error = std::make_error_code(std::errc::operation_not_supported);
        return false; // FIFOs, sockets and device nodes are not ordinary files.
    }
    const auto modified = fs::last_write_time(source, error);
    if (error) return false;
    fs::last_write_time(destination, modified, error);
    if (error) return false;
    fs::permissions(destination, status.permissions(), error);
    return !error;
}
} // namespace
bool validFileName(const QString& name) {
    return !name.isEmpty() && name != "." && name != ".." && !name.contains('/') &&
           !name.contains(QChar(0)) && name.toUtf8().size() <= 255;
}
QString performFileOperation(const QString& operation, const QStringList& paths, const QString& destination) {
    if (operation != "copy" && operation != "move" && operation != "trash") return translate("Unknown file operation.");
    if (paths.isEmpty() || paths.size() > 256) return translate("Select between 1 and 256 items.");
    const QFileInfo destinationInfo(destination);
    if (operation != "trash" && !destinationInfo.isDir()) return translate("Destination folder does not exist.");
    QSet<QString> targets;
    QStringList sources;
    for (const auto& path : paths) sources << QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    // Preflight the complete batch before changing anything.
    for (const auto& path : sources) {
        const QFileInfo source(path);
        if (!exists(path)) return translate("Source no longer exists: %1").arg(path);
        if (source.isRoot()) return translate("The filesystem root cannot be moved or copied.");
        for (const auto& other : sources)
            if (other != path && QFileInfo(other).isDir() && !QFileInfo(other).isSymLink() && inside(path, other))
                return translate("Do not select a folder and its contents in the same operation.");
        if (operation == "trash") continue;
        const auto target = QDir(destinationInfo.absoluteFilePath()).filePath(source.fileName());
        if (targets.contains(target) || exists(target))
            return translate("An item already exists; nothing will be overwritten: %1").arg(target);
        targets.insert(target);
        if (source.isDir() && !source.isSymLink() && inside(destinationInfo.canonicalFilePath(), source.canonicalFilePath()))
            return translate("A folder cannot be copied or moved into itself.");
    }
    for (const auto& path : sources) {
        const auto target = QDir(destinationInfo.absoluteFilePath()).filePath(QFileInfo(path).fileName());
        std::error_code error;
        bool success = false;
        if (operation == "copy") {
            QTemporaryDir staging(QDir(destinationInfo.absoluteFilePath()).filePath(".lunadash-copy-XXXXXX"));
            if (!staging.isValid()) return translate("Could not create a temporary copy in the destination folder.");
            const auto staged = staging.filePath("payload");
            success = copyEntry(nativePath(path), nativePath(staged), error) && renameNoReplace(staged, target, error);
            // QTemporaryDir removes only our uncommitted partial copy on failure.
        } else if (operation == "move") {
            success = renameNoReplace(path, target, error);
        } else {
            success = QFile::moveToTrash(path);
        }
        if (!success) return translate("Operation stopped at: %1. Earlier items may have completed. Cross-filesystem moves may require copy and trash.").arg(path) +
            (error ? "\n" + QString::fromStdString(error.message()) : QString{});
    }
    return {};
}
} // namespace LuDash
