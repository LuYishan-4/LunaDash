#include "shell/runtime/ShellTool.hpp"
#include "desktop/wallpaper/WallpaperActions.hpp"
#include "config/localization/Localization.hpp"
#include "shell/media/MediaPlayer.hpp"
#include "shell/audio/AudioSpectrum.hpp"
#include <string>
#include <vector>

namespace LunaDash {
namespace {
int printNativeJson(const nlohmann::json &object) {
  const auto json = object.dump();
  fwrite(json.data(), 1, json.size(), stdout);
  fputc('\n', stdout);
  return object.contains("error") ? 2 : 0;
}
} // namespace

int ShellTool::run(int argc, char **argv) {
  std::vector<std::string> arguments;
  arguments.reserve(static_cast<std::size_t>(argc));
  for (int index = 0; index < argc; ++index)
    arguments.emplace_back(argv[index]);

  if (arguments.size() < 2)
    return printNativeJson(
        {{"error", "Expected media-status, media-action, audio-spectrum, wallpaper, or language."}});

  if (arguments[1] == "audio-spectrum")
    return runAudioSpectrum();

  if (arguments[1] == "media-status") {
    return printNativeJson(mediaStatus(
        arguments.size() > 2 ? arguments[2] : std::string{},
        arguments.size() > 3 ? arguments[3] : std::string{}));
  }

  if (arguments[1] == "media-action") {
    if (arguments.size() < 3)
      return printNativeJson({{"error", "Expected a media action."}});
    return printNativeJson(mediaAction(
        arguments[2], arguments.size() > 3 ? arguments[3] : "",
        arguments.size() > 4 ? arguments[4] : "",
        arguments.size() > 5 ? arguments[5] : "",
        arguments.size() > 6 ? arguments[6] : ""));
  }

  if (arguments[1] == "wallpaper") {
    std::vector<std::string> actionArguments(arguments.begin() + 2,
                                             arguments.end());
    return printNativeJson(wallpaperAction(actionArguments));
  }

  if (arguments[1] == "language") {
    if (arguments.size() != 3)
      return printNativeJson({{"error", "Expected a locale code."}});
    const std::string &locale = arguments[2];
    std::string error;
    if (!setSelectedLanguage(locale, &error))
      return printNativeJson({{"error", error}});
    return printNativeJson({{"language", locale}});
  }

  return printNativeJson({{"error", "Unknown shell tool command."}});
}

} // namespace LunaDash
