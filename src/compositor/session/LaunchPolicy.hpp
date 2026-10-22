#pragma once

#include <QString>
#include <QStringList>

namespace LunaDash {

struct LaunchIdentity {
  QString desktopId;
  QString flatpakId;
  QString executable;
};

struct LaunchCapabilities {
  bool x11Helper = false;
};

LaunchIdentity identifyLaunch(const QString &desktopId,
                              const QStringList &command);
LaunchCapabilities resolveLaunchCapabilities(const QString &desktopId,
                                             const QStringList &command);

} // namespace LunaDash
