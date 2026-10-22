#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <functional>
class QObject;

namespace LunaDash {
QProcessEnvironment createClientEnvironment(const QString &socketName,
                                            const QString &controlPath,
                                            const QString &binaryDirectory);
bool publishClientEnvironment(const QProcessEnvironment &environment);
// Publish the display variables to the session's D-Bus activation environment
// and, when one exists, to the systemd user manager. Clients started through
// D-Bus activation or a systemd user unit inherit that environment instead of
// the compositor's own, so without this step they start with no display. Only a
// real login session calls this. lunadash-session keeps the login user's
// existing bus when one is available and only creates a private bus for
// minimal/manual launches.
void publishActivationEnvironment(
    const QProcessEnvironment &environment, QObject *owner,
    std::function<void(bool, const QString &)> completion);

// Refresh already-running portal services after the new Wayland/display
// environment has reached the user service manager. try-restart is used so
// inactive services remain D-Bus activated on demand.
void refreshScreencastPortalServices(
    const QProcessEnvironment &environment, QObject *owner,
    std::function<void()> completion = {});
} // namespace LunaDash
