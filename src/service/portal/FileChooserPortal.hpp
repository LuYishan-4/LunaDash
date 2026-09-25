#pragma once
#include <QDBusObjectPath>
#include <QDBusContext>
#include <QObject>
#include <QVariantMap>

namespace LunaDash {
class FileChooserPortal final : public QObject, public QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
public slots:
  uint OpenFile(const QDBusObjectPath &, const QString &, const QString &,
                const QString &title, const QVariantMap &options,
                QVariantMap &results);
  uint SaveFile(const QDBusObjectPath &, const QString &, const QString &,
                const QString &title, const QVariantMap &options,
                QVariantMap &results);
  uint SaveFiles(const QDBusObjectPath &, const QString &, const QString &,
                 const QString &title, const QVariantMap &options,
                 QVariantMap &results);
};
} // namespace LunaDash
