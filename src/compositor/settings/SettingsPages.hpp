#pragma once

#include "core/templates/NamedRegistry.hpp"

#include <string>

namespace LunaDash {

// A page open-settings may ask the shell to show.
struct SettingsPage {
  // Who provides the page: the LunaDash Settings app or the session shell.
  std::string provider;
  // The page shown when no page is named.
  bool defaultPage = false;
};

using SettingsPageRegistry = Templates::NamedRegistry<SettingsPage>;

// The Settings app's pages and the LunaDash-Shell's own settings pages.
void registerBuiltinSettingsPages(SettingsPageRegistry &registry);
// The default page's name, or an empty string.
std::string defaultSettingsPage(const SettingsPageRegistry &registry);

} // namespace LunaDash
