#include "service/portal/Portal.hpp"
#include "config/localization/Localization.hpp"
#include "service/portal/FileChooserPortal.hpp"
#include <QApplication>
#include <QDBusConnection>

namespace LunaDash {
int Portal::run(int argc, char **argv) {
  // A portal backend must not query its own frontend while the frontend is
  // waiting for this service to acquire its bus name.
  qputenv("QT_QPA_PLATFORMTHEME", "generic");
  qunsetenv("GTK_USE_PORTAL");
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  app.setApplicationName(QStringLiteral("LunaDash File Chooser"));
  app.setOrganizationName(QStringLiteral("LunaDash"));
  LunaDash::initializeLocalization(app);

  auto bus = QDBusConnection::sessionBus();
  if (!bus.isConnected())
    return 2;
  if (!bus.registerService(
          QStringLiteral("org.freedesktop.impl.portal.desktop.lunadash")))
    return 3;

  FileChooserPortal portal;
  if (!bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"),
                          &portal, QDBusConnection::ExportAllSlots))
    return 4;
  return app.exec();
}

} // namespace LunaDash
