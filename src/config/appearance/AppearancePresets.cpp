#include "config/appearance/AppearancePresets.hpp"

#include "config/desktop/DesktopPreferenceStore.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace LunaDash {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr std::size_t kPresetLimit = 64;
constexpr std::uintmax_t kMaximumPresetBytes = 16 * 1024;
constexpr std::array<std::string_view, 16> kAppearanceKeys = {
    "accent",
    "secondaryAccent",
    "themeMode",
    "wallpaperColors",
    "gap",
    "panelHeight",
    "blur",
    "blurRadius",
    "windowOpacity",
    "animations",
    "animationDuration",
    "fontFamily",
    "eyeCare",
    "eyeCareTemperature",
    "dockEnabled",
    "dockAutoHide"};

void setError(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
}

fs::path presetsDirectory() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME");
      xdg && *xdg && fs::path(xdg).is_absolute())
    return fs::path(xdg) / "lunadash/presets";
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config/lunadash/presets";
  return {};
}

bool validName(std::string_view name) {
  if (name.empty() || name.size() > 48 ||
      !std::isalnum(static_cast<unsigned char>(name.front())))
    return false;
  return std::all_of(name.begin(), name.end(), [](unsigned char character) {
    return std::isalnum(character) || character == '_' || character == '-';
  });
}

bool writeAll(int descriptor, std::string_view bytes) {
  while (!bytes.empty()) {
    const ssize_t count = ::write(descriptor, bytes.data(), bytes.size());
    if (count < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (count == 0)
      return false;
    bytes.remove_prefix(static_cast<std::size_t>(count));
  }
  return true;
}

class PresetLock final {
public:
  PresetLock(const fs::path &directory, std::string *error) {
    std::error_code filesystemError;
    fs::create_directories(directory, filesystemError);
    if (filesystemError) {
      setError(error, "Could not create the appearance preset directory.");
      return;
    }
    const auto lockPath = directory / ".lock";
    descriptor_ = ::open(lockPath.c_str(),
                         O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor_ < 0) {
      setError(error, "Could not open the appearance preset lock.");
      return;
    }
    while (::flock(descriptor_, LOCK_EX) != 0) {
      if (errno == EINTR)
        continue;
      setError(error, "Could not lock appearance presets.");
      ::close(descriptor_);
      descriptor_ = -1;
      return;
    }
  }

  ~PresetLock() {
    if (descriptor_ >= 0) {
      ::flock(descriptor_, LOCK_UN);
      ::close(descriptor_);
    }
  }

  PresetLock(const PresetLock &) = delete;
  PresetLock &operator=(const PresetLock &) = delete;

  explicit operator bool() const { return descriptor_ >= 0; }

private:
  int descriptor_ = -1;
};

bool isRegularFileWithoutSymlink(const fs::path &path) {
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  return !error && fs::is_regular_file(status);
}

bool writePreset(const fs::path &path, const Json &document,
                 std::string *error) {
  const auto bytes = document.dump(2) + "\n";
  if (bytes.size() > kMaximumPresetBytes) {
    setError(error, "Appearance preset exceeds the 16 KiB size limit.");
    return false;
  }

  std::error_code filesystemError;
  if (fs::exists(path, filesystemError) && !isRegularFileWithoutSymlink(path)) {
    setError(error, "The appearance preset path is not a regular file.");
    return false;
  }
  if (filesystemError) {
    setError(error, "Could not inspect the appearance preset path.");
    return false;
  }

  static std::atomic_uint64_t nextTemporary{0};
  const auto temporary = path.string() + ".tmp." + std::to_string(::getpid()) +
                         "." + std::to_string(nextTemporary.fetch_add(1));
  const int descriptor =
      ::open(temporary.c_str(),
             O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (descriptor < 0) {
    setError(error, "Could not create a temporary appearance preset.");
    return false;
  }
  bool success = writeAll(descriptor, bytes) && ::fsync(descriptor) == 0;
  const int closeResult = ::close(descriptor);
  success = success && closeResult == 0;
  if (success)
    success = ::rename(temporary.c_str(), path.c_str()) == 0;
  if (!success) {
    ::unlink(temporary.c_str());
    setError(error, "Could not atomically save the appearance preset.");
    return false;
  }

  const int directoryDescriptor =
      ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directoryDescriptor >= 0) {
    ::fsync(directoryDescriptor);
    ::close(directoryDescriptor);
  }
  return true;
}

bool readPreset(std::string_view name, Json *values, std::string *error) {
  if (!validName(name)) {
    setError(error,
             "Preset names use 1–48 letters, digits, underscores or hyphens.");
    return false;
  }
  const auto directory = presetsDirectory();
  if (directory.empty()) {
    setError(error, "Could not locate the XDG configuration directory.");
    return false;
  }
  const auto path = directory / (std::string(name) + ".json");
  if (!isRegularFileWithoutSymlink(path)) {
    setError(error, "Appearance preset was not found.");
    return false;
  }
  std::error_code filesystemError;
  const auto size = fs::file_size(path, filesystemError);
  if (filesystemError || size > kMaximumPresetBytes) {
    setError(error, "Appearance preset is unreadable or exceeds 16 KiB.");
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    setError(error, "Could not open the appearance preset.");
    return false;
  }
  try {
    const auto root = Json::parse(input);
    if (!root.is_object() || !root.contains("schemaVersion") ||
        !root.at("schemaVersion").is_number_integer() ||
        root.at("schemaVersion") != 1 || !root.contains("appearance") ||
        !root.at("appearance").is_object() || root.at("appearance").empty()) {
      setError(error, "Invalid appearance preset.");
      return false;
    }
    const auto &appearance = root.at("appearance");
    if (appearance.size() > kAppearanceKeys.size()) {
      setError(error, "Appearance preset contains too many settings.");
      return false;
    }
    for (auto it = appearance.begin(); it != appearance.end(); ++it) {
      if (std::find(kAppearanceKeys.begin(), kAppearanceKeys.end(), it.key()) ==
          kAppearanceKeys.end()) {
        setError(error, "Unknown preset setting: " + it.key());
        return false;
      }
    }
    *values = appearance;
    return true;
  } catch (const Json::exception &) {
    setError(error, "Invalid appearance preset JSON.");
    return false;
  }
}
} // namespace

Json appearancePresets() {
  Json result = Json::array();
  const auto directory = presetsDirectory();
  if (directory.empty())
    return result;
  std::error_code filesystemError;
  fs::directory_iterator iterator(directory, filesystemError);
  if (filesystemError)
    return result;

  std::vector<std::string> names;
  for (const auto &entry : iterator) {
    if (names.size() >= kPresetLimit)
      break;
    const auto path = entry.path();
    if (path.extension() != ".json" || !isRegularFileWithoutSymlink(path))
      continue;
    const auto name = path.stem().string();
    if (validName(name))
      names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  for (const auto &name : names)
    result.push_back({{"name", name}});
  return result;
}

bool saveAppearancePreset(std::string_view name, std::string *error) {
  if (error)
    error->clear();
  if (!validName(name)) {
    setError(error,
             "Preset names use 1–48 letters, digits, underscores or hyphens.");
    return false;
  }
  const auto directory = presetsDirectory();
  if (directory.empty()) {
    setError(error, "Could not locate the XDG configuration directory.");
    return false;
  }
  PresetLock lock(directory, error);
  if (!lock)
    return false;

  const auto path = directory / (std::string(name) + ".json");
  if (appearancePresets().size() >= kPresetLimit && !fs::exists(path)) {
    setError(error, "Keep at most 64 appearance presets.");
    return false;
  }
  const auto preferences = loadDesktopPreferences();
  Json values = Json::object();
  for (const auto key : kAppearanceKeys) {
    const auto found = preferences.find(key);
    if (found != preferences.end())
      values[std::string(key)] = *found;
  }
  return writePreset(path, {{"schemaVersion", 1}, {"appearance", values}},
                     error);
}

bool applyAppearancePreset(std::string_view name, std::string *error) {
  if (error)
    error->clear();
  Json values;
  if (!readPreset(name, &values, error))
    return false;
  return saveDesktopPreferences(values, error);
}

bool deleteAppearancePreset(std::string_view name, std::string *error) {
  if (error)
    error->clear();
  if (!validName(name)) {
    setError(error,
             "Preset names use 1–48 letters, digits, underscores or hyphens.");
    return false;
  }
  const auto directory = presetsDirectory();
  if (directory.empty()) {
    setError(error, "Could not locate the XDG configuration directory.");
    return false;
  }
  PresetLock lock(directory, error);
  if (!lock)
    return false;
  const auto path = directory / (std::string(name) + ".json");
  if (!isRegularFileWithoutSymlink(path)) {
    setError(error, "Appearance preset was not found.");
    return false;
  }
  std::error_code filesystemError;
  if (!fs::remove(path, filesystemError) || filesystemError) {
    setError(error, "Could not delete the appearance preset.");
    return false;
  }
  return true;
}

} // namespace LunaDash
