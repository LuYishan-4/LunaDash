#include "compositor/window/WindowRules.hpp"
#include "compositor/client/ClientWindow.hpp"

namespace LunaDash {

QString windowIconName(const QString &appId, const QString &title) {
  if (appId == "lunadash-app") {
    const auto name = title.toLower();
    if (name.contains("terminal") || name.contains("console"))
      return "utilities-terminal";
    if (name.contains("file"))
      return "system-file-manager";
    if (name.contains("setting"))
      return "preferences-system";
    return "lunadash";
  }
  if (appId == "org.freedesktop.Xwayland")
    return "application-x-executable";
  return appId.isEmpty() ? QStringLiteral("application-x-executable") : appId;
}

bool windowUsesManagedLayout(const ClientWindow &client) {
  return (client.mapped || client.layoutPending) && !client.floating &&
         !client.utility;
}

bool windowAllowsPointerInteraction(const WindowTemplate &,
                                    const ClientWindow &client) {
  return client.mapped && !client.utility && !client.desktop &&
         !client.fullscreen;
}

bool windowAllowsClientMoveResize(const WindowTemplate &windowTemplate,
                                  const ClientWindow &client) {
  return windowAllowsPointerInteraction(windowTemplate, client) &&
         (client.floating || windowTemplate.clientMoveResize);
}

bool windowActivationTogglesMaximize(const WindowTemplate &windowTemplate,
                                     const ClientWindow &client) {
  return windowTemplate.activationTogglesMaximize && !client.floating;
}

bool windowHiddenByMaximize(const WindowTemplate &windowTemplate,
                            LayoutWindowId maximized, bool inMaximizedFamily,
                            const ClientWindow &client) {
  return !windowTemplate.allowOverlap && maximized && !inMaximizedFamily &&
         !client.desktop && !client.floating;
}

} // namespace LunaDash
