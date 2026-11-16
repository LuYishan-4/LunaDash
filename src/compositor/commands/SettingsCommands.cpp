#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/settings/SettingsApi.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/appearance/AppearancePresets.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/PreferenceKeys.hpp"
#include "config/localization/Localization.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include "desktop/theme/ThemeSync.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "desktop/weather/WeatherStatus.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace LunaDash {
namespace {

constexpr std::size_t maximumSettingsRequestBytes = 24 * 1024;

// Preferences the weather service reads; changing one refreshes it.
constexpr const char *weatherPreferences[] = {
    Preference::weatherEnabled.name, Preference::weatherLatitude.name,
    Preference::weatherLongitude.name, Preference::weatherLocation.name};

void synchronizeThemeWithPreferences() {
  const auto preferences = desktopPreferences();
  synchronizeApplicationTheme(preferences, appearancePalette(preferences));
}

} // namespace

void CompositorCommands::registerSettingsCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  registry.add("language", [](C &, const R &r) -> CommandResult {
    if (!isSupportedLanguage(r.value))
      return invalidCommand();
    std::string error;
    if (!setSelectedLanguage(r.value, &error))
      return commandError(error);
    return {};
  });
  // While the shortcut editor records a key, shortcuts are not executed.
  registry.add("shortcut-capture", [](C &c, const R &r) -> CommandResult {
    if (r.value != "true" && r.value != "false")
      return invalidCommand();
    c.shortcutCapture_ = r.value == "true";
    return {};
  });
  registry.add("shortcuts", [](C &c, const R &r) -> CommandResult {
    const auto changes = nlohmann::json::parse(r.value, nullptr, false);
    std::string error;
    if (changes.is_discarded() || !c.shortcutSettings_->apply(changes, &error))
      return commandError(error.empty() ? "Expected shortcut JSON." : error);
    return {};
  });
  registry.add("reset-shortcuts", [](C &c, const R &) -> CommandResult {
    std::string error;
    if (!c.shortcutSettings_->reset(&error))
      return commandError(error);
    return {};
  });
  registry.add("open-settings", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!c.openSettingsPage(r.value, &error))
      return commandError(error);
    return {};
  });
  registry.add("settings-describe", [](C &c, const R &) -> CommandResult {
    return jsonObject(c.state(), "settingsApi");
  });
  registry.add("settings-update", [](C &c, const R &r) -> CommandResult {
    c.pluginManager_->refresh();
    c.arrange();
    if (r.value.size() > maximumSettingsRequestBytes)
      return commandError("Settings request exceeds 24 KiB.");
    const auto document = parseJsonObject(r.value);
    if (!document.is_object() || !c.windowTemplate_)
      return commandError("Expected a settings update object.");
    const auto layout = Settings::target(
        "layout:" + c.windowTemplate_->key, "Window layout", "layout", "Windows",
        c.windowTemplate_->layoutSettingsSchema, c.currentWindowLayoutSettings());
    std::string error;
    if (!updateSettingsApi(
            *c.pluginManager_, layout, document,
            [&c](const auto &changes, std::string *message) {
              return c.updateWindowLayoutSettings(changes, message);
            },
            &error))
      return Json{{"error", error}, {"settingsTarget", jsonValue(document, "target")}};
    c.arrange();
    auto result = c.state();
    result["settingsTarget"] = jsonValue(document, "target");
    return result;
  });
  registry.add("reset-preferences", [](C &c, const R &) -> CommandResult {
    ensureLegacyDesktopSettingsImported();
    std::string error;
    if (!resetDesktopPreferences(&error))
      return commandError(error);
    c.weatherStatus_->refresh();
    if (c.d) {
      c.d->loadOutputSizePreferences();
      c.d->updateNightLight();
    }
    c.applyKeyboardConfiguration();
    c.arrange();
    return {};
  });
  registry.add("appearance", [](C &c, const R &r) -> CommandResult {
    const auto changes = parseJsonObject(r.value);
    if (!changes.is_object())
      return commandError("Expected a JSON object of desktop preferences.");
    std::string error;
    if (!updateDesktopPreferences(changes, &error))
      return commandError(error);
    // Publish color-scheme immediately so portal-aware Chromium/Electron/Qt
    // clients and gsettings-aware GTK applications follow the desktop switch
    // without waiting for the next shell status poll.
    synchronizeThemeWithPreferences();
    c.applyKeyboardConfiguration();
    for (const auto *key : weatherPreferences)
      if (changes.contains(key)) {
        c.weatherStatus_->refresh();
        break;
      }
    if (jsonBool(changes, "wallpaperColors")) {
      const auto path = wallpaperImagePath();
      refreshWallpaperPalette(c.d->eventLoop, path, wallpaperIsVideo(path));
    }
    if (c.d) {
      c.d->loadOutputSizePreferences();
      c.d->updateBackground();
      c.d->updateNightLight();
    }
    c.arrange();
    return {};
  });
  const auto preset = [](bool (*apply)(std::string_view, std::string *),
                         bool synchronizeTheme) {
    return [apply, synchronizeTheme](C &c, const R &r) -> CommandResult {
      ensureLegacyDesktopSettingsImported();
      std::string error;
      if (!apply(r.value, &error))
        return commandError(error);
      if (synchronizeTheme)
        synchronizeThemeWithPreferences();
      if (c.d)
        c.d->updateNightLight();
      c.arrange();
      return {};
    };
  };
  registry.add("appearance-preset-save", preset(saveAppearancePreset, false));
  registry.add("appearance-preset-apply", preset(applyAppearancePreset, true));
  registry.add("appearance-preset-delete", preset(deleteAppearancePreset, false));
  registry.add("eye-care", [](C &c, const R &r) -> CommandResult {
    if (r.value != "toggle" && r.value != "true" && r.value != "false")
      return commandError("Eye care expects toggle, true or false.");
    const bool enabled =
        r.value == "toggle"
            ? !preferenceValue(desktopPreferences(), Preference::eyeCare)
            : r.value == "true";
    std::string error;
    if (!updateDesktopPreferences({{Preference::eyeCare.name, enabled}}, &error))
      return commandError(error);
    if (c.d)
      c.d->updateNightLight();
    c.arrange();
    return {};
  });
  registry.add("finish-setup", [](C &, const R &) -> CommandResult {
    std::string error;
    if (!setSetupComplete(true, &error))
      return commandError(error);
    return {};
  });
  registry.add("setup", [](C &c, const R &) -> CommandResult {
    std::string error;
    if (!setSetupComplete(false, &error))
      return commandError(error);
    c.spawn({"--app", "welcome"});
    return {};
  });
}

} // namespace LunaDash
