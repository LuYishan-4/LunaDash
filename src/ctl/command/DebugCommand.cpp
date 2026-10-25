#include "ctl/command/DebugCommand.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace LunaDash {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;

constexpr const char *kUsage =
    "Usage: lunadashctl debug [COMMAND]\n"
    "  (none)          readable summary: outputs, input focus, cursor, layers, helpers\n"
    "  json            the complete diagnostics as JSON\n"
    "  TOPIC           one section as JSON: input cursor outputs layers windows capture\n"
    "                  processes eventLoop protocols\n"
    "  watch [SECONDS] print cursor, focus and frame changes every second (Ctrl+C stops)\n"
    "  log [LINES]     the end of the newest session log (default 80 lines)\n"
    "  bundle [DIR]    write diagnostics, status, log and environment for a bug report\n";

std::optional<Json> query(ControlClient &client, const std::string &method,
                          const std::string &value = {}) {
  const auto answer = client.request(method, value, 3000);
  if (!answer)
    return std::nullopt;
  auto json = Json::parse(answer->substr(0, answer->find('\n')), nullptr, false);
  if (json.is_discarded() || !json.is_object())
    return std::nullopt;
  return json;
}

std::string text(const Json &value) {
  if (value.is_null())
    return "none";
  if (value.is_string())
    return value.get<std::string>();
  return value.dump();
}

// "window 12 firefox (Mozilla Firefox)", "layer lunadash-panel (top)", "none".
std::string owner(const Json &surface) {
  if (!surface.is_object())
    return "none";
  const auto kind = surface.value("kind", "surface");
  if (kind == "window")
    return "window " + text(surface["id"]) + " " + surface.value("appId", "") + " (" +
           surface.value("title", "") + ")";
  if (kind == "layer")
    return "layer " + surface.value("namespace", "") + " (" + surface.value("layer", "") + ")";
  return kind;
}

void printSummary(const Json &debug) {
  std::cout << "LunaDash " << text(debug["version"]) << " (" << text(debug["commit"])
            << "), up " << debug.value("uptimeSeconds", 0) << " s, renderer "
            << text(debug["renderer"]) << "\n\nOutputs\n";
  for (const auto &output : debug["outputs"])
    std::cout << "  " << text(output["name"]) << "  " << output.value("width", 0) << "x"
              << output.value("height", 0) << " @ "
              << std::fixed << std::setprecision(2)
              << output.value("refreshMilliHz", 0) / 1000.0 << " Hz, scale "
              << output.value("scale", 1.0) << (output.value("enabled", false) ? "" : ", disabled")
              << ", frames " << output.value("frames", 0) << " (slow "
              << output.value("slowFrames", 0) << ", max " << output.value("maxFrameWorkUsec", 0)
              << " us)" << (output.value("softwareCursor", false) ? ", software cursor" : "")
              << "\n";
  const auto &input = debug["input"];
  std::cout << "\nInput\n  keyboards      ";
  for (const auto &keyboard : input["keyboards"])
    std::cout << text(keyboard["name"]) << (keyboard.value("virtual", false) ? " (virtual)" : "")
              << "; ";
  std::cout << "\n  pointers       " << input.value("pointerDevices", 0)
            << "\n  keyboard focus " << owner(input["keyboardFocus"])
            << "\n  pointer focus  " << owner(input["pointerFocus"])
            << "\n  grab           " << (input.value("pointerGrab", false) ? "yes" : "no")
            << ", window operation " << text(input["windowPointerOperation"]) << "\n";
  const auto &cursor = debug["cursor"];
  std::cout << "\nCursor\n  position " << std::setprecision(0) << cursor.value("x", 0.0) << ","
            << cursor.value("y", 0.0) << "  image " << text(cursor["image"]) << "  theme "
            << text(cursor["theme"]) << " " << cursor.value("size", 0) << "\n\nLayers\n";
  for (const auto &layer : debug["layers"])
    std::cout << "  " << std::left << std::setw(28) << text(layer["namespace"]) << std::setw(11)
              << text(layer["layer"]) << (layer.value("mapped", false) ? "mapped  " : "hidden  ")
              << layer.value("width", 0) << "x" << layer.value("height", 0) << ", keyboard "
              << text(layer["keyboard"]) << "\n";
  std::cout << "\nWindows  " << debug["windows"].value("mapped", 0) << " mapped of "
            << debug["windows"].value("count", 0) << "\n";
  const auto &capture = debug["capture"];
  std::cout << "Capture  " << text(capture["phase"]);
  if (!capture.value("last", std::string()).empty())
    std::cout << ", last " << text(capture["last"]);
  if (!capture.value("error", std::string()).empty())
    std::cout << ", error: " << text(capture["error"]);
  std::cout << "\n\nHelpers\n";
  for (const auto &process : debug["processes"])
    std::cout << "  " << std::setw(32) << text(process["program"])
              << (process.value("running", false) ? "running, pid " + text(process["pid"])
                                                    : "finished")
              << "\n";
  std::cout << "\nProtocols  ";
  for (const auto &[name, available] : debug["protocols"].items())
    std::cout << name << (available.get<bool>() ? " " : " (missing) ");
  const auto &loop = debug["eventLoop"];
  std::cout << "\nEvent loop " << loop.value("dispatches", 0) << " dispatches, "
            << loop.value("slowDispatches", 0) << " slow, max " << loop.value("maxDispatchUsec", 0)
            << " us\n";
}

volatile std::sig_atomic_t interrupted = 0;

int watch(ControlClient &client, int seconds) {
  std::signal(SIGINT, [](int) { interrupted = 1; });
  std::string previous;
  long previousSlow = -1;
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (!interrupted && (seconds <= 0 || std::chrono::steady_clock::now() < end)) {
    const auto debug = query(client, "debug");
    if (!debug) {
      std::cerr << "The compositor did not answer.\n";
      return 1;
    }
    long slow = 0;
    for (const auto &output : (*debug)["outputs"])
      slow += output.value("slowFrames", 0L);
    std::ostringstream line;
    const auto &cursor = (*debug)["cursor"];
    line << "cursor " << static_cast<int>(cursor.value("x", 0.0)) << ","
         << static_cast<int>(cursor.value("y", 0.0)) << " " << text(cursor["image"])
         << " | pointer " << owner((*debug)["input"]["pointerFocus"]) << " | keyboard "
         << owner((*debug)["input"]["keyboardFocus"]) << " | capture "
         << text((*debug)["capture"]["phase"]);
    if (line.str() != previous || (previousSlow >= 0 && slow != previousSlow)) {
      const auto now = std::time(nullptr);
      std::cout << std::put_time(std::localtime(&now), "%H:%M:%S ") << line.str();
      if (previousSlow >= 0 && slow != previousSlow)
        std::cout << " | +" << (slow - previousSlow) << " slow frames";
      std::cout << std::endl;
      previous = line.str();
    }
    previousSlow = slow;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}

fs::path stateDirectory() {
  if (const char *state = std::getenv("XDG_STATE_HOME"); state && *state)
    return fs::path(state) / "lunadash";
  const char *home = std::getenv("HOME");
  return fs::path(home ? home : "/") / ".local/state/lunadash";
}

std::optional<fs::path> newestSessionLog() {
  std::optional<fs::path> newest;
  fs::file_time_type newestTime{};
  std::error_code error;
  for (fs::directory_iterator entry(stateDirectory(), error), end; !error && entry != end;
       entry.increment(error)) {
    const auto name = entry->path().filename().string();
    if (!name.starts_with("session-") || !name.ends_with(".log"))
      continue;
    const auto time = entry->last_write_time(error);
    if (!error && (!newest || time > newestTime)) {
      newest = entry->path();
      newestTime = time;
    }
  }
  return newest;
}

std::deque<std::string> tail(const fs::path &path, size_t lines) {
  std::ifstream file(path);
  std::deque<std::string> kept;
  for (std::string line; std::getline(file, line);) {
    kept.push_back(std::move(line));
    if (kept.size() > lines)
      kept.pop_front();
  }
  return kept;
}

size_t number(int argc, char **argv, size_t fallback) {
  if (argc < 2)
    return fallback;
  char *end = nullptr;
  const long value = std::strtol(argv[1], &end, 10);
  return end && *end == '\0' && value > 0 ? static_cast<size_t>(value) : fallback;
}

int bundle(ControlClient &client, int argc, char **argv) {
  const auto now = std::time(nullptr);
  std::ostringstream name;
  name << "lunadash-debug-" << std::put_time(std::localtime(&now), "%Y%m%d-%H%M%S");
  const fs::path directory = (argc >= 2 ? fs::path(argv[1]) : fs::current_path()) / name.str();
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) {
    std::cerr << "Cannot create " << directory << ": " << error.message() << "\n";
    return 1;
  }
  const auto write = [&](const char *file, const std::string &contents) {
    std::ofstream(directory / file) << contents;
  };
  const auto debug = query(client, "debug");
  write("debug.json", debug ? debug->dump(2) + "\n" : "{\"error\":\"no answer\"}\n");
  // The language value suppresses the translation table in the answer.
  const auto status = query(client, "status", "en_US");
  write("status.json", status ? status->dump(2) + "\n" : "{\"error\":\"no answer\"}\n");
  if (const auto log = newestSessionLog()) {
    std::ostringstream lines;
    for (const auto &line : tail(*log, 2000))
      lines << line << "\n";
    write("session.log", lines.str());
  }
  // Only display and session variables: never the full environment (tokens, keys).
  std::ostringstream environment;
  for (const char *variable :
       {"XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "XDG_SESSION_DESKTOP", "WAYLAND_DISPLAY",
        "DISPLAY", "LANG", "LUNADASH_CONTROL", "LUDASH_GRAPHICS", "LUDASH_SHELL_RENDERER",
        "SLINTLU_RENDERER", "XCURSOR_THEME", "XCURSOR_SIZE", "WLR_RENDERER", "WLR_BACKENDS"})
    if (const char *value = std::getenv(variable))
      environment << variable << "=" << value << "\n";
  write("environment.txt", environment.str());
  std::cout << "Wrote " << directory.string() << "\n"
            << "Check the files before sharing them: logs can contain window titles and paths.\n";
  return debug ? 0 : 1;
}
} // namespace

int DebugCommand::run(int argc, char **argv) {
  const std::string command = argc > 0 ? argv[0] : "";
  if (command == "help" || command == "--help" || command == "-h") {
    std::cout << kUsage;
    return 0;
  }
  if (command == "log") {
    const auto log = newestSessionLog();
    if (!log) {
      std::cerr << "No session log in " << stateDirectory().string() << "\n";
      return 1;
    }
    std::cerr << "==> " << log->string() << "\n";
    for (const auto &line : tail(*log, number(argc, argv, 80)))
      std::cout << line << "\n";
    return 0;
  }
  if (ControlClient::socketPath().empty()) {
    std::cerr << "No LunaDash control socket found; is a LunaDash session running?\n";
    return 2;
  }
  if (command == "watch")
    return watch(client_, static_cast<int>(number(argc, argv, 0)));
  if (command == "bundle")
    return bundle(client_, argc, argv);

  const auto debug = query(client_, "debug");
  if (!debug) {
    std::cerr << "The compositor did not answer the debug request.\n";
    return 1;
  }
  if (debug->contains("error")) {
    std::cerr << text((*debug)["error"]) << "\n";
    return 1;
  }
  if (command.empty()) {
    printSummary(*debug);
    return 0;
  }
  if (command == "json") {
    std::cout << debug->dump(2) << "\n";
    return 0;
  }
  if (debug->contains(command)) {
    std::cout << (*debug)[command].dump(2) << "\n";
    return 0;
  }
  std::cerr << "Unknown debug command: " << command << "\n" << kUsage;
  return 2;
}

} // namespace LunaDash
