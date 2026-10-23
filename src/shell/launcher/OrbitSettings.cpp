#include "shell/launcher/OrbitSettings.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {

constexpr std::size_t maximumDocumentBytes = 32 * 1024;

std::string settingsPath() {
  std::filesystem::path path(g_get_user_config_dir());
  path /= "lunadash/orbit.json";
  return path.string();
}

std::filesystem::path defaultsPath() {
  const std::filesystem::path source(LUDASH_ORBIT_DEFAULTS_SOURCE);
  if (std::filesystem::exists(source))
    return source;
  return std::filesystem::path(LUDASH_ORBIT_DEFAULTS_INSTALL);
}

nlohmann::json defaults() {
  static const auto value = [] {
    std::ifstream file(defaultsPath(), std::ios::binary);
    if (!file)
      throw std::runtime_error("Missing Orbit configuration defaults");
    auto document = nlohmann::json::parse(file, nullptr, false);
    if (document.is_discarded() || !document.is_object())
      throw std::runtime_error("Invalid Orbit configuration defaults");
    return document;
  }();
  return value;
}

bool shortText(const nlohmann::json &value, std::size_t maximum = 128) {
  if (!value.is_string())
    return false;
  const auto &text = value.get_ref<const std::string &>();
  if (text.empty() || text.size() > maximum * 4 ||
      text.find('\0') != std::string::npos ||
      text.find('\n') != std::string::npos ||
      !g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr))
    return false;
  glong read = 0;
  glong utf16Units = 0;
  GError *error = nullptr;
  auto *utf16 = g_utf8_to_utf16(text.data(), static_cast<glong>(text.size()),
                                &read, &utf16Units, &error);
  if (error)
    g_error_free(error);
  const bool valid = utf16 && read == static_cast<glong>(text.size()) &&
                     utf16Units >= 0 &&
                     static_cast<std::size_t>(utf16Units) <= maximum;
  g_free(utf16);
  return valid;
}

bool webUrl(const nlohmann::json &value) {
  if (!shortText(value, 2048))
    return false;
  const auto &text = value.get_ref<const std::string &>();
  GError *error = nullptr;
  auto *uri = g_uri_parse(text.c_str(), G_URI_FLAGS_PARSE_RELAXED, &error);
  if (error)
    g_error_free(error);
  if (!uri)
    return false;
  const auto *scheme = g_uri_get_scheme(uri);
  const auto *host = g_uri_get_host(uri);
  const bool valid = scheme && host && *host &&
                     (g_ascii_strcasecmp(scheme, "http") == 0 ||
                      g_ascii_strcasecmp(scheme, "https") == 0);
  g_uri_unref(uri);
  return valid;
}

bool itemsValid(const nlohmann::json &items, int depth) {
  if (!items.is_array() || items.empty() || items.size() > 8 || depth > 2)
    return false;
  std::set<std::string> ids;
  for (const auto &item : items) {
    if (!item.is_object() || !shortText(item.value("id", nlohmann::json{})) ||
        !shortText(item.value("name", nlohmann::json{})))
      return false;
    const auto id = item.at("id").get<std::string>();
    if (!ids.insert(id).second)
      return false;
    int actions = 0;
    for (auto field = item.begin(); field != item.end(); ++field) {
      const auto &key = field.key();
      const auto &value = field.value();
      if (key == "id" || key == "name")
        continue;
      if (key == "description" || key == "icon") {
        if (!shortText(value, 256))
          return false;
        continue;
      }
      ++actions;
      if (key == "children") {
        if (!itemsValid(value, depth + 1))
          return false;
      } else if (key == "url") {
        if (!webUrl(value))
          return false;
      } else if (key == "desktopId") {
        if (!shortText(value, 256))
          return false;
      } else if (key == "action") {
        static const std::set<std::string> allowed{
            "terminal", "files", "browser", "settings", "wallpapers",
            "eye-care", "scratchpad", "power", "clipboard", "dashboard"};
        if (!value.is_string() ||
            !allowed.contains(value.get_ref<const std::string &>()))
          return false;
      } else if (key == "command") {
        if (!value.is_array() || value.empty() || value.size() > 24)
          return false;
        for (const auto &argument : value)
          if (!shortText(argument, 1024))
            return false;
      } else {
        return false;
      }
    }
    if (actions != 1)
      return false;
  }
  return true;
}

bool validate(std::string_view text, nlohmann::json *object,
              std::string *error) {
  const auto fail = [error](std::string message) {
    if (error)
      *error = std::move(message);
    return false;
  };
  if (!object || text.size() > maximumDocumentBytes)
    return fail("Orbit configuration must be at most 32 KiB.");
  const auto root = nlohmann::json::parse(text.begin(), text.end(), nullptr, false);
  const auto fieldsAllowed = root.is_object() && root.size() == 4 &&
      root.contains("schemaVersion") && root.contains("defaultEngine") &&
      root.contains("searchEngines") && root.contains("items");
  if (!fieldsAllowed || !root.at("schemaVersion").is_number_integer() ||
      root.at("schemaVersion") != 1 || !root.at("items").is_array() ||
      !itemsValid(root.at("items"), 0) ||
      !root.at("searchEngines").is_array())
    return fail("Orbit requires schemaVersion 1, a defaultEngine, 1–8 searchEngines "
                "and 1–8 items per folder (at most three levels). Each item needs "
                "one action, desktopId, command array, web URL or children.");

  const auto &engines = root.at("searchEngines");
  if (engines.empty() || engines.size() > 8)
    return fail("Orbit requires schemaVersion 1, a defaultEngine, 1–8 searchEngines "
                "and 1–8 items per folder (at most three levels). Each item needs "
                "one action, desktopId, command array, web URL or children.");
  std::set<std::string> ids;
  for (const auto &engine : engines) {
    if (!engine.is_object() || engine.size() != 3 ||
        !shortText(engine.value("id", nlohmann::json{})) ||
        !shortText(engine.value("name", nlohmann::json{})) ||
        !webUrl(engine.value("url", nlohmann::json{})))
      return fail("Orbit requires schemaVersion 1, a defaultEngine, 1–8 searchEngines "
                  "and 1–8 items per folder (at most three levels). Each item needs "
                  "one action, desktopId, command array, web URL or children.");
    const auto id = engine.at("id").get<std::string>();
    const auto url = engine.at("url").get<std::string>();
    const auto first = url.find("{query}");
    if (first == std::string::npos ||
        url.find("{query}", first + 1) != std::string::npos ||
        !ids.insert(id).second)
      return fail("Orbit requires schemaVersion 1, a defaultEngine, 1–8 searchEngines "
                  "and 1–8 items per folder (at most three levels). Each item needs "
                  "one action, desktopId, command array, web URL or children.");
  }
  if (!root.at("defaultEngine").is_string() ||
      !ids.contains(root.at("defaultEngine").get<std::string>()))
    return fail("Orbit requires schemaVersion 1, a defaultEngine, 1–8 searchEngines "
                "and 1–8 items per folder (at most three levels). Each item needs "
                "one action, desktopId, command array, web URL or children.");
  *object = root;
  return true;
}

bool writeAtomically(const std::string &path, std::string_view bytes,
                     std::string *error) {
  struct stat info {};
  const int statResult = ::lstat(path.c_str(), &info);
  if (statResult == 0 && S_ISLNK(info.st_mode)) {
    if (error)
      *error = "Refusing to overwrite a symlinked Orbit configuration.";
    return false;
  }
  if (statResult != 0 && errno != ENOENT) {
    if (error)
      *error = "Could not save Orbit configuration atomically.";
    return false;
  }

  std::string temporary = path + ".tmp.XXXXXX";
  std::vector<char> name(temporary.begin(), temporary.end());
  name.push_back('\0');
  const int fd = ::mkstemp(name.data());
  if (fd < 0) {
    if (error)
      *error = "Could not save Orbit configuration atomically.";
    return false;
  }
  const std::string temporaryPath(name.data());
  bool ok = ::fchmod(fd, S_IRUSR | S_IWUSR) == 0;
  std::size_t offset = 0;
  while (ok && offset < bytes.size()) {
    const auto count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0) {
      ok = false;
      break;
    }
    offset += static_cast<std::size_t>(count);
  }
  if (ok)
    ok = ::fsync(fd) == 0;
  if (::close(fd) != 0)
    ok = false;
  if (ok)
    ok = ::rename(temporaryPath.c_str(), path.c_str()) == 0;
  if (!ok) {
    ::unlink(temporaryPath.c_str());
    if (error)
      *error = "Could not save Orbit configuration atomically.";
  }
  return ok;
}

std::uint64_t revision = 0;

} // namespace

nlohmann::json orbitSettings() {
  static nlohmann::json cache;
  static std::int64_t cachedModified = -2;
  static std::int64_t cachedSize = -2;
  static std::uint64_t cachedRevision = std::numeric_limits<std::uint64_t>::max();

  const auto path = settingsPath();
  std::error_code fileError;
  const bool exists = std::filesystem::exists(path, fileError) && !fileError;
  std::int64_t modified = -1;
  std::int64_t size = -1;
  if (exists) {
    const auto time = std::filesystem::last_write_time(path, fileError);
    if (!fileError)
      modified = time.time_since_epoch().count();
    const auto fileSize = std::filesystem::file_size(path, fileError);
    if (!fileError && fileSize <= static_cast<std::uintmax_t>(INT64_MAX))
      size = static_cast<std::int64_t>(fileSize);
  }
  if (cachedRevision == revision && cachedModified == modified &&
      cachedSize == size)
    return cache;

  auto document = defaults();
  std::string error;
  if (fileError) {
    error = "Could not read Orbit configuration.";
  } else if (exists) {
    std::ifstream file(path, std::ios::binary);
    std::string text(maximumDocumentBytes + 1, '\0');
    if (!file.is_open()) {
      error = "Could not read Orbit configuration.";
    } else {
      file.read(text.data(), static_cast<std::streamsize>(text.size()));
      text.resize(static_cast<std::size_t>(file.gcount()));
      if (file.bad() || !validate(text, &document, &error)) {
        if (error.empty())
          error = "Could not read Orbit configuration.";
      }
    }
  }
  cache = {{"document", std::move(document)}, {"path", path}, {"error", error}};
  cachedModified = modified;
  cachedSize = size;
  cachedRevision = revision;
  return cache;
}

bool saveOrbitSettings(std::string_view text, std::string *error) {
  nlohmann::json document;
  if (!validate(text, &document, error))
    return false;
  const auto path = settingsPath();
  std::error_code fileError;
  std::filesystem::create_directories(
      std::filesystem::path(path).parent_path(), fileError);
  if (fileError) {
    if (error)
      *error = "Could not create Orbit configuration directory.";
    return false;
  }
  if (!writeAtomically(path, document.dump(2), error))
    return false;
  ++revision;
  return true;
}

bool resetOrbitSettings(std::string *error) {
  return saveOrbitSettings(defaults().dump(), error);
}

} // namespace LunaDash
