#include "desktop/wallpaper/WallpaperActions.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"

namespace LunaDash {
nlohmann::json wallpaperAction(const std::vector<std::string> &arguments) {
  std::string error;
  bool ok = false;
  if (arguments.size() == 1 && arguments[0] == "refresh")
    ok = refreshWallpaperLibrary(&error);
  else if (arguments.size() == 1 && arguments[0] == "create-directory")
    ok = createWallpaperDirectory(&error);
  else if (arguments.size() == 3 && arguments[0] == "favorite" &&
           (arguments[2] == "true" || arguments[2] == "false"))
    ok = setWallpaperFavorite(arguments[1], arguments[2] == "true", &error);
  else
    return {{"error", "Expected refresh, create-directory or favorite PATH true|false."}};
  return ok ? nlohmann::json{{"ok", true}}
            : nlohmann::json{{"error", error}};
}
} // namespace LunaDash
