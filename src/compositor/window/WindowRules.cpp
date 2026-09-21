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
    if (name.contains("monitor"))
      return "utilities-system-monitor";
    return "lunadash";
  }
  if (appId == "org.freedesktop.Xwayland")
    return "application-x-executable";
  return appId.isEmpty() ? QStringLiteral("application-x-executable") : appId;
}

bool windowUsesManagedLayout(const ClientWindow &client) {
  return client.mapped && !client.floating && !client.utility;
}

bool windowAllowsPointerInteraction(const WindowTemplate &,
                                    const ClientWindow &client) {
  return windowUsesManagedLayout(client);
}

bool windowAllowsClientMoveResize(const WindowTemplate &windowTemplate,
                                  const ClientWindow &client) {
  return windowTemplate.clientMoveResize && windowUsesManagedLayout(client);
}

bool windowActivationTogglesMaximize(const WindowTemplate &windowTemplate,
                                     const ClientWindow &client) {
  return windowTemplate.activation == WindowActivationTemplate::ToggleMaximize &&
         !client.floating;
}

bool windowHiddenByMaximize(const WindowTemplate &windowTemplate,
                            LayoutWindowId maximized, bool inMaximizedFamily,
                            const ClientWindow &client) {
  return windowTemplate.layoutMode == WindowLayoutMode::Tiling && maximized &&
         !inMaximizedFamily && !client.desktop;
}

} // namespace LunaDash
