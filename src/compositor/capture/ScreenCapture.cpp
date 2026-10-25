#include "compositor/capture/ScreenCapture.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <string_view>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {
std::string findExecutable(std::string_view name,
                           const ProcessEnvironment &environment) {
  if (name.empty())
    return {};
  if (name.find('/') != std::string_view::npos) {
    return ::access(std::string(name).c_str(), X_OK) == 0
               ? std::string(name)
               : std::string{};
  }

  const auto pathValue = environment.find("PATH");
  const char *path = pathValue == environment.end()
                         ? std::getenv("PATH")
                         : pathValue->second.c_str();
  if (!path)
    return {};

  std::string_view directories(path);
  while (true) {
    const auto separator = directories.find(':');
    const auto directory = directories.substr(0, separator);
    const auto candidate =
        (directory.empty() ? std::filesystem::path(".")
                           : std::filesystem::path(directory)) /
        name;
    if (::access(candidate.c_str(), X_OK) == 0)
      return candidate.string();
    if (separator == std::string_view::npos)
      break;
    directories.remove_prefix(separator + 1);
  }
  return {};
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

bool validGeometry(const std::string &geometry) {
  static const std::regex pattern(
      R"(^-?[0-9]+,-?[0-9]+ ([1-9][0-9]*)x([1-9][0-9]*)$)");
  std::smatch match;
  if (!std::regex_match(geometry, match, pattern))
    return false;

  unsigned width = 0;
  unsigned height = 0;
  const auto widthText = match[1].str();
  const auto heightText = match[2].str();
  const auto widthResult = std::from_chars(
      widthText.data(), widthText.data() + widthText.size(), width);
  const auto heightResult = std::from_chars(
      heightText.data(), heightText.data() + heightText.size(), height);
  return widthResult.ec == std::errc{} &&
         widthResult.ptr == widthText.data() + widthText.size() &&
         heightResult.ec == std::errc{} &&
         heightResult.ptr == heightText.data() + heightText.size() &&
         width <= 32768 && height <= 32768;
}

std::string diagnostic(const std::string &output) {
  auto message = trim(output);
  if (message.size() > 1024)
    message.resize(1024);
  return message;
}
} // namespace

ScreenCapture::ScreenCapture(wl_event_loop *eventLoop) : process_(eventLoop) {}

ScreenCapture::~ScreenCapture() {
  process_.cancel();
  removeTemporary();
}

void ScreenCapture::setCompletion(Completion completion) {
  completion_ = std::move(completion);
}

bool ScreenCapture::busy() const {
  return process_.busy() || phase_ == "selecting" || phase_ == "capturing";
}

const std::string &ScreenCapture::phase() const { return phase_; }

bool ScreenCapture::prepareDestination(const std::string &path,
                                       std::string *error) {
  const std::filesystem::path destination(path);
  std::error_code filesystemError;
  if (!destination.is_absolute() ||
      std::filesystem::exists(destination, filesystemError) ||
      filesystemError) {
    *error = "Screenshot destination must be a new absolute path.";
    return false;
  }
  return true;
}

bool ScreenCapture::createTemporary(std::string *error) {
  const auto directory = std::filesystem::path(destination_).parent_path();
  auto pattern = (directory / ".lunadash-capture-XXXXXX").string();
  std::vector<char> path(pattern.begin(), pattern.end());
  path.push_back('\0');
  const int descriptor = ::mkstemp(path.data());
  if (descriptor < 0) {
    *error = "Could not create the screenshot file.";
    return false;
  }
  temporaryPath_ = path.data();
  // mkstemp creates owner-only files; a saved screenshot gets the usual permissions.
  const mode_t mask = ::umask(0);
  ::umask(mask);
  const bool permissions = ::fchmod(descriptor, 0666 & ~mask) == 0;
  if (::close(descriptor) != 0 || !permissions) {
    removeTemporary();
    *error = "Could not create the screenshot file.";
    return false;
  }
  return true;
}

bool ScreenCapture::captureOutput(const ProcessEnvironment &environment,
                                  const std::string &path,
                                  std::string *error) {
  if (busy()) {
    *error = "A screenshot capture is already active.";
    return false;
  }
  grim_ = findExecutable("grim", environment);
  if (grim_.empty()) {
    *error = "Install grim to capture the compositor output.";
    return false;
  }
  if (!prepareDestination(path, error))
    return false;

  destination_ = path;
  if (!createTemporary(error))
    return false;
  environment_ = environment;
  phase_ = "capturing";
  if (!process_.runDetailed(
          grim_, {temporaryPath_},
          [this](const ProcessResult &result) { processFinished(result); },
          15000, environment_)) {
    phase_ = "idle";
    removeTemporary();
    destination_.clear();
    *error = "Could not start the screenshot helper.";
    return false;
  }
  return true;
}

bool ScreenCapture::selectRegion(const ProcessEnvironment &environment,
                                 const std::string &path,
                                 std::string *error) {
  if (busy()) {
    *error = "A screenshot selection is already active.";
    return false;
  }
  const auto selector = findExecutable("slurp", environment);
  grim_ = findExecutable("grim", environment);
  if (selector.empty() || grim_.empty()) {
    *error = "Install grim and slurp to select a screenshot region.";
    return false;
  }
  if (!prepareDestination(path, error))
    return false;

  environment_ = environment;
  destination_ = path;
  phase_ = "selecting";
  // No deadline: choosing a region takes as long as the user needs (Escape cancels).
  if (!process_.runDetailed(
          selector, {"-f", "%x,%y %wx%h"},
          [this](const ProcessResult &result) { processFinished(result); },
          0, environment_)) {
    phase_ = "idle";
    destination_.clear();
    *error = "Could not start the screenshot selector.";
    return false;
  }
  return true;
}

void ScreenCapture::finish(std::string path, std::string error) {
  phase_ = !error.empty() ? "failed" : path.empty() ? "cancelled" : "saved";
  if (path.empty())
    removeTemporary();
  else
    temporaryPath_.clear();
  destination_.clear();
  if (completion_)
    completion_(path, error);
}

void ScreenCapture::processFinished(const ProcessResult &result) {
  const auto output = trim(result.output);
  if (phase_ == "selecting") {
    if (!result.success && result.exited && result.exitCode == 1 &&
        (output.empty() || output == "selection cancelled")) {
      finish({}, {});
      return;
    }
    if (!result.success || !result.exited || result.exitCode != 0 ||
        !validGeometry(output)) {
      const auto detail = diagnostic(output);
      finish({}, "Could not select a screenshot region." +
                     (detail.empty() ? std::string{} : " " + detail));
      return;
    }
    std::string error;
    if (!createTemporary(&error)) {
      finish({}, error);
      return;
    }
    phase_ = "capturing";
    if (!process_.runDetailed(
            grim_, {"-g", output, temporaryPath_},
            [this](const ProcessResult &captureResult) {
              processFinished(captureResult);
            },
            15000, environment_))
      finish({}, "Could not start the screenshot helper.");
    return;
  }

  std::string error;
  if (!result.success || !result.exited || result.exitCode != 0 ||
      !saveTemporary(&error)) {
    if (error.empty())
      error = "Could not save the screenshot." +
              (output.empty() ? std::string{} : " " + diagnostic(output));
    finish({}, error);
    return;
  }
  finish(destination_, {});
}

bool ScreenCapture::saveTemporary(std::string *error) {
  std::error_code filesystemError;
  const auto size = std::filesystem::file_size(temporaryPath_, filesystemError);
  if (filesystemError || size == 0) {
    *error = "Screenshot helper produced an empty file.";
    return false;
  }
  if (::link(temporaryPath_.c_str(), destination_.c_str()) != 0) {
    *error = "Could not save the screenshot without overwriting a file.";
    return false;
  }
  if (::unlink(temporaryPath_.c_str()) != 0) {
    removeTemporary();
    *error = "Could not finalize the screenshot file.";
    return false;
  }
  temporaryPath_.clear();
  return true;
}

void ScreenCapture::removeTemporary() {
  if (!temporaryPath_.empty()) {
    ::unlink(temporaryPath_.c_str());
    temporaryPath_.clear();
  }
}

} // namespace LunaDash
