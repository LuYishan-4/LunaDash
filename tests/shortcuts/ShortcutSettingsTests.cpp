#include "config/desktop/DesktopPreferenceStore.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

class TemporaryConfig final {
public:
  TemporaryConfig() {
    std::string pattern = "/tmp/lunadash-shortcuts-XXXXXX";
    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');
    char *created = ::mkdtemp(path.data());
    if (!created)
      throw std::runtime_error("Could not create temporary configuration.");
    path_ = created;
    const char *existing = std::getenv("XDG_CONFIG_HOME");
    if (existing)
      previous_ = existing;
  }

  ~TemporaryConfig() {
    if (previous_)
      ::setenv("XDG_CONFIG_HOME", previous_->c_str(), 1);
    else
      ::unsetenv("XDG_CONFIG_HOME");
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  void use(const std::string &name) const {
    const auto directory = path_ / name;
    std::filesystem::create_directories(directory);
    if (::setenv("XDG_CONFIG_HOME", directory.c_str(), 1) != 0)
      throw std::runtime_error("Could not set XDG_CONFIG_HOME.");
  }

private:
  std::filesystem::path path_;
  std::optional<std::string> previous_;
};

void importsLegacyBindingsAndKeepsValidValues(const TemporaryConfig &config) {
  config.use("import");
  const nlohmann::json legacy{{"launchTerminal", "Meta+Return"},
                              {"closeWindow", "Meta+T"},
                              {"toggleFloating", "Meta+Space"}};
  LunaDash::ShortcutSettings settings(legacy);
  require(settings.actionFor(XKB_KEY_t, LunaDash::ShortcutMeta) ==
              "closeWindow",
          "Imported Meta+T binding was lost.");
  require(settings.actionFor(XKB_KEY_Return, LunaDash::ShortcutMeta) ==
              "launchTerminal",
          "Imported Meta+Return binding was lost.");
  const auto imported = LunaDash::loadShortcutBindings();
  require(!imported.contains("toggleFloating"),
          "Obsolete shortcut action was kept.");

  LunaDash::ShortcutSettings subsequent;
  require(subsequent.actionFor(XKB_KEY_t, LunaDash::ShortcutMeta) ==
              "closeWindow",
          "Persisted shortcut binding was not reloaded.");
  require(subsequent.actionFor(XKB_KEY_q, LunaDash::ShortcutMeta).empty(),
          "Unassigned shortcut unexpectedly matched an action.");
}

void updatesAndResetUseNativeStore(const TemporaryConfig &config) {
  config.use("updates");
  LunaDash::ShortcutSettings settings;
  std::string error;
  require(!settings.apply({{"openPowerMenu", "Alt+Tab"}}, &error),
          "Reserved Alt+Tab binding was accepted.");
  require(settings.apply({{"openPowerMenu", "Alt+P"}}, &error),
          "Valid shortcut update failed.");
  require(settings.actionFor(XKB_KEY_p, LunaDash::ShortcutAlt) ==
              "openPowerMenu",
          "Updated shortcut was not applied.");

  require(settings.reset(&error), "Reset could not persist defaults.");
  require(settings.actionFor(XKB_KEY_t, LunaDash::ShortcutMeta) ==
              "launchTerminal",
          "Reset did not restore the default binding.");
  require(LunaDash::loadShortcutBindings().empty(),
          "Reset left persisted shortcut overrides behind.");

  LunaDash::ShortcutSettings afterReset;
  require(afterReset.actionFor(XKB_KEY_t, LunaDash::ShortcutMeta) ==
              "launchTerminal",
          "Reset default was not retained by the next settings instance.");
}

} // namespace

int main() {
  try {
    TemporaryConfig config;
    importsLegacyBindingsAndKeepsValidValues(config);
    updatesAndResetUseNativeStore(config);
  } catch (const std::exception &error) {
    std::cerr << "Shortcut settings test failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
