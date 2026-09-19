#include "desktop/fileoperations/FileOperations.hpp"
#include "config/localization/Localization.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <system_error>
#include <unistd.h>

namespace LunaDash {
namespace {
namespace fs = std::filesystem;
fs::path nativePath(const QString &path) {
  return fs::path(QFile::encodeName(path).constData());
}
bool exists(const QString &path) {
  const QFileInfo info(path);
  return info.exists() || info.isSymLink();
}
bool inside(const QString &path, const QString &directory) {
  return path == directory ||
         path.startsWith(directory.endsWith('/') ? directory : directory + '/');
}
bool renameNoReplace(const QString &source, const QString &destination,
                     std::error_code &error) {
#ifdef SYS_renameat2
  if (::syscall(SYS_renameat2, AT_FDCWD, QFile::encodeName(source).constData(),
                AT_FDCWD, QFile::encodeName(destination).constData(),
                RENAME_NOREPLACE) == 0)
    return true;
  error = std::error_code(errno, std::generic_category());
#else
  error = std::make_error_code(std::errc::operation_not_supported);
#endif
  return false;
}
bool copyEntry(const fs::path &source, const fs::path &destination,
               std::error_code &error, int depth = 0) {
  if (depth > 128) {
    error = std::make_error_code(std::errc::filename_too_long);
    return false;
  }
  const auto status = fs::symlink_status(source, error);
  if (error)
    return false;
  if (fs::is_symlink(status)) {
    fs::copy_symlink(source, destination, error);
    return !error;
  }
  if (fs::is_regular_file(status)) {
    if (!fs::copy_file(source, destination, fs::copy_options::none, error))
      return false;
  } else if (fs::is_directory(status)) {
    if (!fs::create_directory(destination, error))
      return false;
    fs::directory_iterator iterator(source, error), end;
    while (!error && iterator != end) {
      const auto entry = iterator->path();
      if (!copyEntry(entry, destination / entry.filename(), error, depth + 1))
        return false;
      iterator.increment(error);
    }
    if (error)
      return false;
  } else {
    error = std::make_error_code(std::errc::operation_not_supported);
    return false;
  }
  const auto modified = fs::last_write_time(source, error);
  if (error)
    return false;
  fs::last_write_time(destination, modified, error);
  if (error)
    return false;
  fs::permissions(destination, status.permissions(), error);
  return !error;
}
QString duplicateTarget(const QFileInfo &source) {
  const auto directory = source.dir();
  QString stem = source.fileName();
  QString suffix;
  if (source.isFile() && !source.completeSuffix().isEmpty()) {
    suffix = "." + source.completeSuffix();
    stem.chop(suffix.size());
  }
  for (int index = 1; index <= 9999; ++index) {
    const auto label =
        index == 1 ? translate("copy") : translate("copy %1").arg(index);
    const auto candidate =
        directory.filePath(stem + " (" + label + ")" + suffix);
    if (!exists(candidate))
      return candidate;
  }
  return {};
}
} // namespace

bool validFileName(const QString &name) {
  return !name.isEmpty() && name != "." && name != ".." &&
         !name.contains('/') && !name.contains(QChar(0)) &&
         name.toUtf8().size() <= 255;
}

QString performFileOperation(const QString &operation, const QStringList &paths,
                             const QString &destination) {
  if (operation != "copy" && operation != "move" && operation != "trash" &&
      operation != "duplicate" && operation != "delete")
    return translate("Unknown file operation.");
  if (paths.isEmpty() || paths.size() > 256)
    return translate("Select between 1 and 256 items.");

  const QFileInfo destinationInfo(destination);
  if ((operation == "copy" || operation == "move") && !destinationInfo.isDir())
    return translate("Destination folder does not exist.");

  QSet<QString> targets;
  QStringList sources;
  for (const auto &path : paths)
    sources << QDir::cleanPath(QFileInfo(path).absoluteFilePath());

  for (const auto &path : sources) {
    const QFileInfo source(path);
    if (!exists(path))
      return translate("Source no longer exists: %1").arg(path);
    if (source.isRoot())
      return translate(
          "The filesystem root cannot be changed by this operation.");
    for (const auto &other : sources)
      if (other != path && QFileInfo(other).isDir() &&
          !QFileInfo(other).isSymLink() && inside(path, other))
        return translate(
            "Do not select a folder and its contents in the same operation.");

    if (operation == "trash" || operation == "delete" ||
        operation == "duplicate")
      continue;
    const auto target =
        QDir(destinationInfo.absoluteFilePath()).filePath(source.fileName());
    if (targets.contains(target) || exists(target))
      return translate(
                 "An item already exists; nothing will be overwritten: %1")
          .arg(target);
    targets.insert(target);
    if (source.isDir() && !source.isSymLink() &&
        inside(destinationInfo.canonicalFilePath(), source.canonicalFilePath()))
      return translate("A folder cannot be copied or moved into itself.");
  }

  for (const auto &path : sources) {
    const QFileInfo source(path);
    std::error_code error;
    bool success = false;

    if (operation == "copy") {
      const auto target =
          QDir(destinationInfo.absoluteFilePath()).filePath(source.fileName());
      QTemporaryDir staging(QDir(destinationInfo.absoluteFilePath())
                                .filePath(".lunadash-copy-XXXXXX"));
      if (!staging.isValid())
        return translate(
            "Could not create a temporary copy in the destination folder.");
      const auto staged = staging.filePath("payload");
      success = copyEntry(nativePath(path), nativePath(staged), error) &&
                renameNoReplace(staged, target, error);
    } else if (operation == "move") {
      const auto target =
          QDir(destinationInfo.absoluteFilePath()).filePath(source.fileName());
      success = renameNoReplace(path, target, error);
    } else if (operation == "trash") {
      success = QFile::moveToTrash(path);
    } else if (operation == "duplicate") {
      const auto target = duplicateTarget(source);
      if (target.isEmpty())
        return translate("Could not choose a unique duplicate name for: %1")
            .arg(path);
      QTemporaryDir staging(
          source.dir().filePath(".lunadash-duplicate-XXXXXX"));
      if (!staging.isValid())
        return translate("Could not create a temporary duplicate beside: %1")
            .arg(path);
      const auto staged = staging.filePath("payload");
      success = copyEntry(nativePath(path), nativePath(staged), error) &&
                renameNoReplace(staged, target, error);
    } else if (operation == "delete") {
      const auto removed = fs::remove_all(nativePath(path), error);
      success = !error && removed > 0;
    }

    if (!success)
      return translate(
                 "Operation stopped at: %1. Earlier items may have completed.")
                 .arg(path) +
             (error ? "\n" + QString::fromStdString(error.message())
                    : QString{});
  }
  return {};
}
} // namespace LunaDash
