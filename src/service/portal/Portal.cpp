#include "service/portal/Portal.hpp"
#include "config/localization/Localization.hpp"
#include "service/portal/FileChooserPortal.hpp"
#include "service/portal/SettingsPortal.hpp"
#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QDebug>

namespace LunaDash {
int Portal::run(int argc, char **argv) {
  QApplication app(argc, argv);
  QApplication::setStyle("Fusion");
  app.setQuitOnLastWindowClosed(false);
  app.setApplicationName(QStringLiteral("LunaDash"));
  app.setOrganizationName(QStringLiteral("LunaDash"));
  LunaDash::initializeLocalization(app);

  // Register before constructing adaptors or exporting introspection data.
  // Qt must not omit a slot/signal on the first connection and only discover
  // its argument type after a caller has already sent a request.
  qDBusRegisterMetaType<QDBusObjectPath>();
  qDBusRegisterMetaType<QDBusVariant>();
  qDBusRegisterMetaType<PortalSettingsMap>();
  auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    qCritical() << "LunaDash portal: session bus unavailable:" << bus.lastError();
    return 2;
  }

  FileChooserPortal portal;
  new SettingsPortal(&portal);
  const QString path = QStringLiteral("/org/freedesktop/portal/desktop");
  if (!bus.registerObject(path, &portal,
                          QDBusConnection::ExportAllSlots |
                              QDBusConnection::ExportAllProperties |
                              QDBusConnection::ExportAdaptors)) {
    qCritical() << "LunaDash portal: cannot export interfaces:" << bus.lastError();
    return 4;
  }
  // Type=dbus and the frontend treat name acquisition as readiness. Export
  // the object first, so the first caller cannot observe an empty service.
  if (!bus.registerService(
          QStringLiteral("org.freedesktop.impl.portal.desktop.lunadash"))) {
    qCritical() << "LunaDash portal: cannot own backend name:" << bus.lastError();
    return 3;
  }
  qInfo() << "LunaDash portal ready: FileChooser v4, Settings v2, platform"
          << app.platformName();
  return app.exec();
}

} // namespace LunaDash
