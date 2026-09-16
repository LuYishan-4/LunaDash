#pragma once

#include <QProcessEnvironment>
#include <QString>

namespace LuDash {
QProcessEnvironment createClientEnvironment(const QString &socketName,
                                            const QString &controlPath,
                                            const QString &binaryDirectory);
bool publishClientEnvironment(const QProcessEnvironment &environment);
// Publish the display variables to the session's D-Bus activation environment
// and, when one exists, to the systemd user manager. Clients started through
// D-Bus activation or a systemd user unit inherit that environment instead of
// the compositor's own, so without this step they start with no display. Only a
// real login session calls this: lunadash-session runs on a private session
// bus, so the values never reach another desktop.
bool publishActivationEnvironment(const QProcessEnvironment &environment,
                                  QString *error = nullptr);
} // namespace LuDash
