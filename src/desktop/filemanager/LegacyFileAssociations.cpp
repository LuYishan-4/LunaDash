#include "config/localization/Localization.hpp"
#include "desktop/filemanager/FileAssociationUi.hpp"
#include "desktop/filemanager/FileAssociations.hpp"
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>

namespace LunaDash {
bool migrateLegacyFileAssociations(QString *error) {
  if (error)
    error->clear();
  QSettings settings;
  if (settings.value("files/associationMigrationVersion", 0).toInt() >= 1)
    return true;
  FileAssociations store;
  const auto config = store.read(error);
  if (config.isEmpty())
    return false;
  auto existing = config.value("associations").toObject();
  settings.beginGroup("fileAssociations");
  int unavailable = 0;
  for (const auto &oldKey : settings.childKeys()) {
    QString key;
    QString mime;
    if (oldKey.startsWith("ext_")) {
      key = FileAssociations::keyForExtension(oldKey.mid(4));
      mime = QMimeDatabase()
                 .mimeTypeForFile("example." + oldKey.mid(4),
                                  QMimeDatabase::MatchExtension)
                 .name();
    } else if (oldKey.startsWith("mime_")) {
      mime = oldKey.mid(5);
      const auto separator = mime.indexOf('_');
      if (separator >= 0)
        mime[separator] = '/';
      key = "mime:" + mime;
    }
    if (!FileAssociations::validKey(key) || existing.contains(key))
      continue;
    const auto oldValue = settings.value(oldKey).toString();
    const auto id =
        oldValue == "__system__" ? oldValue : QFileInfo(oldValue).fileName();
    if (!fileApplicationAvailable(id)) {
      ++unavailable;
      continue;
    }
    QString failure;
    if (!store.setRule(key, id, mime, &failure)) {
      if (error)
        *error = failure;
      return false;
    }
  }
  settings.endGroup();
  // Preserve the old entries as a backup. A completed migration must not
  // resurrect an association after the user deletes its new JSON rule.
  settings.setValue("files/associationMigrationVersion", 1);
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = translate("Could not save file associations.");
    return false;
  }
  if (unavailable && error)
    *error = translate("Some previous file associations refer to unavailable "
                       "applications. The old settings were kept; choose a new "
                       "application when opening those files.");
  return true;
}
} // namespace LunaDash
