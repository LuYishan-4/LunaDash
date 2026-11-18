#include "compositor/session/ShellLocation.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "core/log/Log.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"

#include <string>
#include <string_view>

namespace LunaDash {
namespace {

// Compositor shortcut actions and the shell request each one stands for.
std::string_view shellRequestFor(std::string_view action) {
  if (action == "launchOrbit")
    return "orbit-launcher";
  if (action == "openControlCenter")
    return "control-center";
  if (action == "openClipboard")
    return "clipboard";
  if (action == "openPowerMenu")
    return "power-menu";
  return action;
}

} // namespace

void WaylandCompositor::sendShellRequest(std::string_view action,
                                         std::string_view argument) {
  // Only the LunaDash shell (SlintLu) takes these; another shell binds its own keys.
  if (session_.shell != SessionConfig::Shell::LunaDash)
    return;
  // Exactly what any compositor's shortcut can do: SlintLu's IPC, with no state kept here.
  const std::string slintlu = findSlintLu();
  if (slintlu.empty()) {
    logWarning("Shell request %.*s: SlintLu was not found.",
               static_cast<int>(action.size()), action.data());
    return;
  }
  spawn({"msg", "call", "Shell.request", std::string(action), std::string(argument)},
        slintlu, false);
}

void WaylandCompositor::requestShellAction(std::string_view action) {
  sendShellRequest(shellRequestFor(action));
}

void WaylandCompositor::setLauncherVisible(bool visible, bool publish) {
  if (launcherVisible_ == visible)
    return;
  launcherVisible_ = visible;
  if (publish)
    sendShellRequest("launcher", visible ? "true" : "false");
}

bool WaylandCompositor::openSettingsPage(const std::string &page,
                                         std::string *error) {
  if (!page.empty() && !settingsPages_.contains(page)) {
    if (error)
      *error = "Unknown settings page.";
    return false;
  }
  sendShellRequest("settings",
                   page.empty() ? defaultSettingsPage(settingsPages_) : page);
  return true;
}

bool WaylandCompositor::openWallpaperPicker(std::string *error) {
  // Super+W is a standalone wallpaper-gallery toggle. Refresh discovery only
  // when the user opens the gallery; background status polls reuse the cache
  // and never rescan a large Pictures/Wallpapers tree on a timer.
  if (!refreshWallpaperLibrary(error))
    return false;
  sendShellRequest("wallpaper-picker");
  return true;
}

} // namespace LunaDash
