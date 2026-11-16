#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"

#include <string>
#include <string_view>

namespace LunaDash {
namespace {

// Serials the shell compares with the last value it handled; 0 means "none yet".
constexpr int maximumShellSerial = 999999; //:D

int nextShellSerial(int serial) {
  return serial >= maximumShellSerial ? 1 : serial + 1;
}

} // namespace

void WaylandCompositor::setLauncherVisible(bool visible, bool publish) {
  if (shellRequests_.launcherVisible == visible)
    return;
  shellRequests_.launcherVisible = visible;
  if (!publish)
    return;
  shellRequests_.launcherSerial = nextShellSerial(shellRequests_.launcherSerial);
  windowSwitcher_->setLauncherState(shellRequests_.launcherSerial,
                                    shellRequests_.launcherVisible);
}

void WaylandCompositor::requestShellAction(std::string_view action) {
  shellRequests_.action = std::string(action);
  shellRequests_.actionSerial = nextShellSerial(shellRequests_.actionSerial);
}

bool WaylandCompositor::openSettingsPage(const std::string &page,
                                         std::string *error) {
  if (!page.empty() && !settingsPages_.contains(page)) {
    if (error)
      *error = "Unknown settings page.";
    return false;
  }
  shellRequests_.settingsPage =
      page.empty() ? defaultSettingsPage(settingsPages_) : page;
  shellRequests_.settingsSerial = nextShellSerial(shellRequests_.settingsSerial);
  return true;
}

bool WaylandCompositor::openWallpaperPicker(std::string *error) {
  // Super+W is a standalone wallpaper-gallery toggle. Refresh discovery only
  // when the user opens the gallery; background status polls reuse the cache
  // and never rescan a large Pictures/Wallpapers tree on a timer.
  if (!refreshWallpaperLibrary(error))
    return false;
  shellRequests_.pickerSerial = nextShellSerial(shellRequests_.pickerSerial);
  return true;
}

} // namespace LunaDash
