#pragma once

#include <QProcessEnvironment>
#include <QString>

namespace LuDash {
QProcessEnvironment createClientEnvironment(const QString &socketName,
                                            const QString &controlPath,
                                            const QString &binaryDirectory);
// Sets the process environment for the compositor and its children, and
// optionally publishes the Wayland/IM identity to the D-Bus and systemd
// activation environment. Pass publishToSession=false when running nested so
// the host session's environment is not polluted.
bool publishClientEnvironment(const QProcessEnvironment &environment,
                              bool publishToSession = true);
} // namespace LuDash
