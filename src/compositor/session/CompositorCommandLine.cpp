#include "compositor/session/CompositorCommandLine.hpp"

#include "compositor/renderer/selection/RenderSelection.h"
#include "config/BuildConfig.hpp"
#include "config/session/SessionConfig.hpp"
#include "core/text/Text.hpp"

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

namespace LunaDash {
namespace {

std::optional<CompositorOptions> parseArguments(int argc, char **argv,
                                                std::string &error) {
  CompositorOptions options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    const auto value = [&](std::string_view option) -> const char * {
      if (argument == option) {
        if (index + 1 >= argc ||
            std::string_view(argv[index + 1]).starts_with("--")) {
          error = "Missing value after '" + std::string(option) + "'.";
          return nullptr;
        }
        return argv[++index];
      }
      const std::string prefix = std::string(option) + "=";
      if (argument.starts_with(prefix))
        return argv[index] + prefix.size();
      error = "Unknown option '" + std::string(argument) + "'.";
      return nullptr;
    };
    if (argument == "--help" || argument == "-h") {
      options.help = true;
    } else if (argument == "--version") {
      options.version = true;
    } else if (argument == "--session-shell") {
      options.sessionShell = true;
    } else if (argument == "--profile") {
      options.profile = true;
    } else if (argument == "--fullscreen") {
      options.fullscreen = true;
    } else if (argument == "--no-shell") {
      options.noShell = true;
    } else if (argument == "--demo") {
      options.demo = true;
    } else if (argument.starts_with("--graphics")) {
      const auto *text = value("--graphics");
      if (!text)
        return std::nullopt;
      options.graphics = text;
    } else if (argument.starts_with("--socket")) {
      const auto *text = value("--socket");
      if (!text)
        return std::nullopt;
      options.socket = text;
    } else if (argument.starts_with("--state")) {
      const auto *text = value("--state");
      if (!text)
        return std::nullopt;
      options.state = text;
    } else if (argument.starts_with("--exit-after")) {
      const auto *text = value("--exit-after");
      if (!text)
        return std::nullopt;
      const std::string_view number(text);
      const auto parsed = std::from_chars(number.data(),
                                          number.data() + number.size(),
                                          options.exitAfterMilliseconds);
      if (parsed.ec != std::errc{} ||
          parsed.ptr != number.data() + number.size()) {
        error = "Invalid value for '--exit-after': " + std::string(number);
        return std::nullopt;
      }
      options.exitAfterSet = true;
    } else {
      error = "Unknown option '" + std::string(argument) + "'.";
      return std::nullopt;
    }
  }

  options.graphics = asciiLower(options.graphics);
  if (!ludash_renderer_preference_valid(options.graphics.c_str())) {
    error = "--graphics must be auto, opengl, gles, or vulkan.";
    return std::nullopt;
  }
  return options;
}

void printHelp() {
  std::cout
      << "Usage: lunadash-compositor [options]\n"
      << "  --help                 Show this help\n"
      << "  --version              Show version\n"
      << "  --session-shell        Print the shell session.json selects (lunadash, custom, none)\n"
      << "  --graphics <api>       Renderer: auto, opengl, gles, or vulkan\n"
      << "  --socket <name>        Wayland socket name\n"
      << "  --profile              Log event-loop stalls longer than 25 ms\n"
      << "  --fullscreen           Prefer host output size in nested mode\n"
      << "  --no-shell             Do not start the desktop shell\n"
      << "  --demo                 Open the Welcome app (needs the shell)\n"
      << "  --state <path>         Write window geometry JSON at exit\n"
      << "  --exit-after <ms>      Exit after this many milliseconds\n";
}

} // namespace

std::optional<CompositorOptions> parseCompositorOptions(int argc, char **argv) {
  std::string error;
  auto options = parseArguments(argc, argv, error);
  if (!options)
    std::cerr << error << "\nUse --help for usage.\n";
  return options;
}

std::optional<int> chooseOption(const CompositorOptions &options) {
  if (options.help) {
    printHelp();
    return 0;
  }
  if (options.version) {
    std::cout << BuildConfig::version << " (" << BuildConfig::commit << ")\n";
    return 0;
  }
  if (options.sessionShell) {
    const auto session = loadSessionConfig();
    if (!session.error.empty())
      std::cerr << session.source.string() << ": " << session.error << '\n';
    std::cout << sessionShellName(session.shell) << '\n';
    return 0;
  }
  return std::nullopt;
}

} // namespace LunaDash
