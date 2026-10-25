#include "compositor/capture/ScreenCapture.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <wayland-server-core.h>

namespace LunaDash {
namespace {

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}

void writeExecutable(const std::filesystem::path &path, const std::string &contents) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << contents;
  ::chmod(path.c_str(), 0755);
}

// A saved capture reports the destination it was written to: the clipboard copy and the
// "Screenshot saved" notification both start from that path.
void savedCaptureReportsItsPath(wl_event_loop *eventLoop, const std::filesystem::path &root) {
  const auto bin = root / "bin";
  writeExecutable(bin / "grim", "#!/bin/sh\nprintf '\\211PNG\\r\\n' > \"$1\"\n");
  const auto destination = root / "Pictures/shot.png";
  std::filesystem::create_directories(destination.parent_path());

  ScreenCapture capture(eventLoop);
  bool completed = false;
  std::string reportedPath;
  std::string reportedError;
  capture.setCompletion([&](const std::string &path, const std::string &error) {
    completed = true;
    reportedPath = path;
    reportedError = error;
  });
  std::string error;
  require(capture.captureOutput({{"PATH", bin.string()}}, destination.string(), &error),
          "Capture did not start: " + error);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!completed && std::chrono::steady_clock::now() < deadline)
    wl_event_loop_dispatch(eventLoop, 20);
  require(completed, "Capture did not finish.");
  require(reportedError.empty(), "Capture failed: " + reportedError);
  require(reportedPath == destination.string(),
          "Completion reported '" + reportedPath + "' instead of the saved file.");
  require(std::filesystem::file_size(destination) > 0, "The screenshot file is empty.");
  require(capture.phase() == "saved", "Phase is " + capture.phase());
}

} // namespace
} // namespace LunaDash

int main() {
  char pattern[] = "/tmp/lunadash-screen-capture-XXXXXX";
  const char *created = ::mkdtemp(pattern);
  if (!created)
    return 1;
  const std::filesystem::path root = created;
  wl_event_loop *eventLoop = wl_event_loop_create();
  int status = 0;
  try {
    LunaDash::savedCaptureReportsItsPath(eventLoop, root);
  } catch (const std::exception &error) {
    std::cerr << "Screen capture test failed: " << error.what() << '\n';
    status = 1;
  }
  wl_event_loop_destroy(eventLoop);
  std::filesystem::remove_all(root);
  return status;
}
