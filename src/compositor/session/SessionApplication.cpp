#include "compositor/session/SessionApplication.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "config/BuildConfig.hpp"
#include "compositor/renderer/selection/RenderSelection.h"

#include <algorithm>
#include <csignal>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>

namespace LunaDash {
namespace {
struct SessionOptions {
  std::string graphics = "auto";
  std::string socket = "lunadash-0";
  std::string screenshot;
  std::string state;
  int exitAfterMilliseconds = 0;
  bool exitAfterSet = false;
  bool nested = false;
  bool profile = false;
  bool fullscreen = false;
  bool noShell = false;
  bool demo = false;
  bool help = false;
  bool version = false;
};

std::optional<SessionOptions> parseOptions(int argc, char **argv) {
  SessionOptions options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    const auto value = [&](std::string_view option) -> const char * {
      if (argument == option) {
        if (index + 1 >= argc)
          return nullptr;
        const char *text = argv[++index];
        return std::string_view(text).starts_with("--") ? nullptr : text;
      }
      const std::string prefix = std::string(option) + "=";
      if (argument.starts_with(prefix))
        return argv[index] + prefix.size();
      return nullptr;
    };
    if (argument == "--help" || argument == "-h") {
      options.help = true;
    } else if (argument == "--version") {
      options.version = true;
    } else if (argument == "--nested") {
      options.nested = true;
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
    } else if (argument.starts_with("--screenshot")) {
      const auto *text = value("--screenshot");
      if (!text)
        return std::nullopt;
      options.screenshot = text;
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
      if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size())
        return std::nullopt;
      options.exitAfterSet = true;
    } else {
      return std::nullopt;
    }
  }
  return options;
}

void printHelp() {
  std::cout
      << "Usage: lunadash-compositor [options]\n"
      << "  --help                 Show this help\n"
      << "  --version              Show version\n"
      << "  --graphics <api>       Renderer: auto, opengl, gles, or vulkan\n"
      << "  --socket <name>        Wayland socket name\n"
      << "  --nested               Run with the nested wlroots backend\n"
      << "  --profile              Log event-loop stalls longer than 25 ms\n"
      << "  --fullscreen           Prefer host output size in nested mode\n"
      << "  --no-shell             Do not start the desktop shell\n"
      << "  --demo                 Start two demonstration clients\n"
      << "  --screenshot <path>    Save a compositor screenshot at exit\n"
      << "  --state <path>         Write window geometry JSON at exit\n"
      << "  --exit-after <ms>      Exit after this many milliseconds\n";
}

void fatalSignalHandler(int signalNumber) {
  static constexpr char message[] =
      "LunaDash compositor received a fatal signal.\n";
  const auto written = ::write(STDERR_FILENO, message, sizeof(message) - 1);
  // Best-effort diagnostic: a failed or partial write must not delay the signal.
  (void)written;
  ::signal(signalNumber, SIG_DFL);
  ::kill(::getpid(), signalNumber);
}

void installFatalSignalDiagnostics() {
  for (const int signalNumber :
       {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE}) {
    struct sigaction action {};
    action.sa_handler = fatalSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;
    sigaction(signalNumber, &action, nullptr);
  }
}
} // namespace

int SessionApplication::run(int argc, char **argv) {
  installFatalSignalDiagnostics();
  const std::string applicationVersion =
      std::string(BuildConfig::version) + " (" + BuildConfig::commit + ")";
  const auto parsedOptions = parseOptions(argc, argv);
  if (!parsedOptions) {
    std::cerr << "Invalid command-line arguments. Use --help for usage.\n";
    return 2;
  }
  const auto &options = *parsedOptions;
  if (options.help) {
    printHelp();
    return 0;
  }
  if (options.version) {
    std::cout << applicationVersion << '\n';
    return 0;
  }

  const QString renderer = QString::fromStdString(options.graphics).toLower();
  if (!ludash_renderer_preference_valid(renderer.toUtf8().constData())) {
    qCritical("--graphics must be auto, opengl, gles, or vulkan");
    return 2;
  }

  std::error_code executablePathError;
  const auto executablePath =
      std::filesystem::read_symlink("/proc/self/exe", executablePathError);
  qInfo().noquote() << "LunaDash version:"
                    << QString::fromStdString(applicationVersion)
                    << "executable:"
                    << QString::fromStdString(executablePath.string());
  qInfo("LunaDash input: wlroots seat + libinput/xkbcommon (no Qt input path)");

  LunaDash::WaylandCompositor compositor(
      QString::fromStdString(options.socket).toUtf8(), options.fullscreen,
      !options.noShell, renderer);

  if (options.demo) {
    if (!compositor.scheduleEventLoopCallback(900, [&] {
          if (!options.noShell)
            compositor.spawn({"--app", "welcome"});
          compositor.spawn({"--app", "files"});
        }))
      return 2;
  }

  if (options.exitAfterSet) {
    if (!compositor.scheduleEventLoopCallback(
            std::max(1000, options.exitAfterMilliseconds), [&] {
              if (!options.state.empty())
                compositor.saveState(
                    QString::fromStdString(options.state));
              const auto finish = [&compositor](bool ok) {
                compositor.closeTestSession(
                    [&compositor, ok](bool clean) {
                      compositor.terminateEventLoop(
                          ok && clean && !compositor.hasProcessFailure() ? 0
                                                                        : 2);
                    });
              };
              if (options.screenshot.empty()) {
                finish(true);
                return;
              }
              if (!compositor.saveScreenshot(
                      QString::fromStdString(options.screenshot), finish))
                finish(false);
            }))
      return 2;
  }

  return compositor.runEventLoop(options.profile);
}

} // namespace LunaDash
