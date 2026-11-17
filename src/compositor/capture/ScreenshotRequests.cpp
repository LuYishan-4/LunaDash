#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/session/DesktopNotification.hpp"
#include "compositor/session/ShellLocation.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "core/filesystem/FileIo.hpp"
#include "core/log/Log.hpp"
#include "core/process/ExecutableLookup.hpp"

#include <glib.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace LunaDash {
namespace {

// Local time with milliseconds, e.g. 20261116-142501-042.
std::string captureTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm localTime{};
  localtime_r(&time, &localTime);
  std::ostringstream result;
  result << std::put_time(&localTime, "%Y%m%d-%H%M%S-")
         << std::setw(3) << std::setfill('0') << milliseconds % 1000;
  return result.str();
}

} // namespace

bool WaylandCompositor::saveScreenshot(const std::string &path) {
  if (!d || !d->primaryOutput || !std::filesystem::path(path).is_absolute() ||
      fileExists(path) || screenCapture_->busy())
    return false;

  capture_.error.clear();
  wlr_output_schedule_frame(d->primaryOutput);
  std::string error;
  if (!screenCapture_->captureOutput(clientEnvironment_, path, &error)) {
    capture_.error = error;
    return false;
  }
  return true;
}

std::string WaylandCompositor::nextCapturePath() const {
  const char *pictures = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
  auto directory =
      pictures ? std::string(pictures)
               : std::string(g_get_home_dir()) + "/Pictures";
  directory += "/Screenshots";
  if (!createDirectories(directory))
    return {};
  return directory + "/LunaDash-" + captureTimestamp() + ".png";
}

void WaylandCompositor::captureScreen() {
  if (screenCapture_->busy())
    return;
  capture_.error.clear();
  const std::string path = nextCapturePath();
  if (path.empty()) {
    capture_.error = "Could not create the screenshot directory.";
    return;
  }
  std::string error;
  if (!screenCapture_->selectRegion(clientEnvironment_, path, &error))
    capture_.error = error;
}

void WaylandCompositor::onScreenCaptureFinished(const std::string &path,
                                                const std::string &error) {
  if (!path.empty()) {
    capture_.last = path;
    const std::string helper = findClipboardHelper();
    if (helper.empty()) {
      logWarning("Screenshot saved, but clipboard helper is unavailable.");
    } else {
      spawn({"publish-file", "image/png", path}, helper, false,
            [](const ProcessResult &result) {
              if (!result.success)
                logWarning("Screenshot saved, but publishing it to the "
                           "clipboard failed.");
            });
    }
  }
  capture_.error = error;
  // Tell the user how it went: the selection ends without any other feedback.
  if (!path.empty())
    sendDesktopNotification("Screenshot saved", path, "image-x-generic");
  else if (!error.empty())
    sendDesktopNotification("Screenshot failed", error, "dialog-error");
  // The selector's surface (and its cursor) are gone: show a cursor again.
  if (d)
    d->schedulePointerRebase();
}

} // namespace LunaDash
