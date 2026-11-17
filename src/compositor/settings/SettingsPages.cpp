#include "compositor/settings/SettingsPages.hpp"

namespace LunaDash {

void registerBuiltinSettingsPages(SettingsPageRegistry &registry) {
  registry.add("general", {"settings", true});
  for (const char *page :
       {"appearance", "windows", "shortcuts", "display", "input", "sound",
        "network", "bluetooth", "power", "applications", "privacy", "system",
        "devices", "about", "modules", "plugins", "dashboard", "input-method"})
    registry.add(page, {"settings"});
  for (const char *page : {"wallpaper", "panel", "notifications"})
    registry.add(page, {"shell"});
}

std::string defaultSettingsPage(const SettingsPageRegistry &registry) {
  for (const auto &name : registry.names())
    if (registry.find(name)->defaultPage)
      return name;
  return {};
}

} // namespace LunaDash
