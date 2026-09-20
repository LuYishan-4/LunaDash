#include "compositor/plugins/PluginBundle.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <algorithm>

namespace LunaDash {
namespace {
QStringList files(const QString &directory, QString *error) {
  const auto root = QFileInfo(directory).canonicalFilePath();
  QStringList result;
  qint64 bytes = 0;
  QDirIterator iterator(root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
  while (iterator.hasNext()) {
    const auto path = iterator.next();
    const QFileInfo info(path);
    bytes += info.size();
    if (!info.canonicalFilePath().startsWith(root + "/") ||
        bytes > 64 * 1024 * 1024 || result.size() >= 512) {
      if (error)
        *error =
            "Plugin bundle escapes its directory or exceeds 64 MiB / 512 files";
      return {};
    }
    result.append(QDir(root).relativeFilePath(path));
  }
  std::sort(result.begin(), result.end());
  return result;
}
} // namespace
QString PluginBundle::fingerprint(const QString &directory, QString *error) {
  const auto names = files(directory, error);
  if (names.isEmpty())
    return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  for (const auto &name : names) {
    const QFileInfo info(QDir(directory).filePath(name));
    hash.addData(name.toUtf8());
    hash.addData(QByteArray::number(info.size()));
    hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
  }
  return QString::fromLatin1(hash.result().toHex());
}
std::shared_ptr<PluginBundle> PluginBundle::copy(const QString &directory,
                                                 QString *error) {
  const auto names = files(directory, error);
  if (names.isEmpty())
    return {};
  auto bundle = std::make_shared<PluginBundle>();
  if (!bundle->directory_.isValid()) {
    if (error)
      *error = "Could not create private plugin revision directory";
    return {};
  }
  for (const auto &name : names) {
    const auto destination = bundle->file(name);
    QDir().mkpath(QFileInfo(destination).absolutePath());
    if (!QFile::copy(QDir(directory).filePath(name), destination)) {
      if (error)
        *error = "Plugin changed while staging its revision; retry after the "
                 "build completes";
      return {};
    }
  }
  return bundle;
}
QString PluginBundle::file(const QString &name) const {
  return QDir(directory_.path()).filePath(name);
}
} // namespace LunaDash
