#include "compositor/window/WindowRules.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "core/text/Text.hpp"

namespace LunaDash {

std::string windowIconName(const std::string &appId, const std::string &title) {
  if (appId == "lunadash-app") {
    const auto name = asciiLower(title);
    if (containsText(name, "terminal") || containsText(name, "console"))
      return "utilities-terminal";
    if (containsText(name, "file"))
      return "system-file-manager";
    if (containsText(name, "setting"))
      return "preferences-system";
    return "lunadash";
  }
  if (appId == "org.freedesktop.Xwayland")
    return "application-x-executable";
  return appId.empty() ? std::string("application-x-executable") : appId;
}

bool windowUsesManagedLayout(const ClientWindow &client) {
  return (client.state.mapped || client.layout.pending) &&
         !client.state.floating && !client.utility;
}

bool windowAllowsPointerInteraction(const WindowTemplate &,
                                    const ClientWindow &client) {
  return client.state.mapped && !client.utility && !client.state.fullscreen;
}

bool windowAllowsClientMoveResize(const WindowTemplate &windowTemplate,
                                  const ClientWindow &client) {
  return windowAllowsPointerInteraction(windowTemplate, client) &&
         (client.state.floating || windowTemplate.clientMoveResize);
}

bool windowActivationTogglesMaximize(const WindowTemplate &windowTemplate,
                                     const ClientWindow &client) {
  return windowTemplate.activationTogglesMaximize && !client.state.floating;
}

bool windowHiddenByMaximize(const WindowTemplate &windowTemplate,
                            LayoutWindowId maximized, bool inMaximizedFamily,
                            const ClientWindow &client) {
  return !windowTemplate.allowOverlap && maximized && !inMaximizedFamily &&
         !client.state.floating;
}

} // namespace LunaDash
