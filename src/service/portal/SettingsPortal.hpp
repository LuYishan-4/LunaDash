#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusVariant>
#include <QMap>
#include <QVariantMap>

namespace LunaDash {
using PortalSettingsMap = QMap<QString, QVariantMap>;
class SettingsPortal final : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")
  Q_PROPERTY(uint version READ version CONSTANT)
public:
  explicit SettingsPortal(QObject *parent);
  uint version() const;
public slots:
  LunaDash::PortalSettingsMap ReadAll(const QStringList &namespaces);
  QDBusVariant Read(const QString &nameSpace, const QString &key);
signals:
  void SettingChanged(const QString &nameSpace, const QString &key,
                      const QDBusVariant &value);

private:
  QVariantMap values_;
  void refresh();
};
} // namespace LunaDash
Q_DECLARE_METATYPE(LunaDash::PortalSettingsMap)
