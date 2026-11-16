#include "compositor/commands/CommandArguments.hpp"
#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "core/text/Text.hpp"
#include "core/text/Url.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace LunaDash {
namespace {

// wallpaper-favorite values: "add:<path>" or "remove:<path>", plain text so
// shells need no JSON escaping.
constexpr std::string_view addFavoritePrefix = "add:";
constexpr std::string_view removeFavoritePrefix = "remove:";

// The palette follows the selected wallpaper (image or video poster).
void refreshPaletteForSelection(wl_event_loop *eventLoop) {
  const auto path = wallpaperImagePath();
  refreshWallpaperPalette(eventLoop, path, wallpaperIsVideo(path));
}

} // namespace

void CompositorCommands::registerWallpaperCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  // Rescans the library without asking the shell to open its picker.
  registry.add("wallpaper-refresh", [](C &, const R &) -> CommandResult {
    std::string error;
    if (!refreshWallpaperLibrary(&error))
      return commandError(error);
    return {};
  });
  registry.add("choose-wallpaper", [](C &c, const R &) -> CommandResult {
    std::string error;
    if (!c.openWallpaperPicker(&error))
      return commandError(error);
    return {};
  });
  registry.add("wallpaper-favorite", [](C &, const R &r) -> CommandResult {
    const std::string_view value = r.value;
    const bool add = value.starts_with(addFavoritePrefix);
    const auto path = add ? value.substr(addFavoritePrefix.size())
                      : value.starts_with(removeFavoritePrefix)
                          ? value.substr(removeFavoritePrefix.size())
                          : std::string_view{};
    std::string error;
    if (path.empty() ||
        !setWallpaperFavorite(std::filesystem::path(path), add, &error))
      return commandError(error.empty() ? "Expected add:<path> or remove:<path>."
                                        : error);
    return {};
  });
  registry.add("wallpaper-random", [](C &c, const R &) -> CommandResult {
    std::string error;
    if (!selectRandomWallpaper(&error))
      return commandError(error);
    refreshPaletteForSelection(c.d->eventLoop);
    return {};
  });
  // A local path or file:// URL.
  registry.add("wallpaper-image", [](C &c, const R &r) -> CommandResult {
    const auto localPath = localPathFromFileUrl(r.value);
    const auto path = !localPath.empty() ? std::filesystem::path(localPath)
                                         : std::filesystem::path(r.value);
    std::string error;
    if (!setWallpaperImage(path, &error))
      return commandError(error);
    refreshPaletteForSelection(c.d->eventLoop);
    return {};
  });
  registry.add("wallpaper-default", [](C &c, const R &) -> CommandResult {
    std::string error;
    if (!resetWallpaperImage(&error))
      return commandError(error);
    refreshPaletteForSelection(c.d->eventLoop);
    return {};
  });
  // Selects a built-in shader wallpaper; the wallpaper settings validate the index.
  registry.add("wallpaper", [](C &c, const R &r) -> CommandResult {
    const auto shader = parseInteger(trimmed(r.value));
    if (!shader || *shader < 0)
      return invalidCommand();
    ensureLegacyDesktopSettingsImported();
    std::string error;
    if (!saveWallpaperSettings({{"shader", *shader}, {"mode", "shader"}}, &error))
      return commandError(error);
    if (c.d)
      c.d->updateBackground();
    return {};
  });
}

} // namespace LunaDash
