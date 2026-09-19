#include "service/portal/Portal.hpp"
#include "config/localization/Localization.hpp"
#include "service/portal/FileChooserPortal.hpp"
#include <QApplication>
#include <QDBusConnection>

namespace LunaDash {
int Portal::run(int argc, char **argv) {
  QApplication app(argc, argv);
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
