#include "desktop/theme/FcitxTheme.hpp"

#include <gio/gio.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace LunaDash {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

std::string errorMessage(std::string_view action, const fs::path &path) {
  return std::string(action) + ": " + path.string() + ": " +
         std::error_code(errno, std::generic_category()).message();
}

bool writeFile(const fs::path &path, std::string_view contents,
               std::string *error) {
  std::error_code filesystemError;
  fs::create_directories(path.parent_path(), filesystemError);
  if (filesystemError) {
    if (error)
      *error = "Could not create theme directory: " +
               filesystemError.message();
    return false;
  }

  static std::atomic_uint64_t nextTemporary{0};
  const auto temporary = path.string() + ".tmp." + std::to_string(::getpid()) +
                         "." + std::to_string(nextTemporary.fetch_add(1));
  const int descriptor =
      ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
  if (descriptor < 0) {
    if (error)
      *error = errorMessage("Could not create theme file", path);
    return false;
  }
  std::string_view remaining = contents;
  bool success = true;
  while (!remaining.empty()) {
    const auto written = ::write(descriptor, remaining.data(), remaining.size());
    if (written < 0) {
      if (errno == EINTR)
        continue;
      success = false;
      break;
    }
    if (written == 0) {
      success = false;
      break;
    }
    remaining.remove_prefix(static_cast<std::size_t>(written));
  }
  success = success && ::fsync(descriptor) == 0;
  const int closeResult = ::close(descriptor);
  success = success && closeResult == 0;
  if (success)
    success = ::rename(temporary.c_str(), path.c_str()) == 0;
  if (!success) {
    const auto failure = errorMessage("Could not save theme file", path);
    ::unlink(temporary.c_str());
    if (error)
      *error = failure;
    return false;
  }
  return true;
}

bool readFile(const fs::path &path, std::string &contents,
              std::string *error) {
  std::error_code filesystemError;
  const auto size = fs::file_size(path, filesystemError);
  if (filesystemError) {
    if (filesystemError == std::errc::no_such_file_or_directory) {
      contents.clear();
      return true;
    }
    if (error)
      *error = "Could not inspect theme configuration: " +
               filesystemError.message();
    return false;
  }
  if (size > 1024 * 1024) {
    if (error)
      *error = "Theme configuration exceeds the 1 MiB limit: " +
               path.string();
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (error)
      *error = "Could not read theme configuration: " + path.string();
    return false;
  }
  contents.assign(std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>());
  return !input.bad();
}

bool backupOnce(const fs::path &path, std::string *error) {
  const auto backup = fs::path(path.string() + ".lunadash-backup");
  std::error_code filesystemError;
  if (fs::exists(backup, filesystemError))
    return true;
  if (filesystemError) {
    if (error)
      *error = "Could not inspect backup path: " +
               filesystemError.message();
    return false;
  }
  fs::copy_file(path, backup, fs::copy_options::none, filesystemError);
  if (filesystemError) {
    if (error)
      *error = "Could not back up configuration: " + path.string() + ": " +
               filesystemError.message();
    return false;
  }
  return true;
}

std::string paletteColor(const Json &palette, std::string_view key) {
  const auto found = palette.find(key);
  return found != palette.end() && found->is_string()
             ? found->get<std::string>()
             : std::string{};
}

void replaceAll(std::string &text, std::string_view needle,
                std::string_view replacement) {
  std::size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    text.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
}

std::string readTemplate(const char *name) {
  const std::array<fs::path, 2> roots{
      LUDASH_THEME_SOURCE_DIR,
      LUDASH_THEME_INSTALL_DIR,
  };
  for (const auto &root : roots) {
    std::ifstream input(root / "nyxmellow" / name, std::ios::binary);
    if (input)
      return {std::istreambuf_iterator<char>(input),
              std::istreambuf_iterator<char>()};
  }
  return {};
}

void reloadFinished(GObject *source, GAsyncResult *result, gpointer userData) {
  GError *error = nullptr;
  GVariant *reply = g_dbus_connection_call_finish(
      G_DBUS_CONNECTION(source), result, &error);
  if (reply)
    g_variant_unref(reply);
  g_clear_error(&error);
  g_object_unref(userData);
}

void reloadFcitx() {
  GError *error = nullptr;
  GDBusConnection *connection =
      g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
  if (!connection) {
    g_clear_error(&error);
    return;
  }
  g_dbus_connection_call(
      connection, "org.fcitx.Fcitx5", "/controller",
      "org.fcitx.Fcitx.Controller1", "ReloadAddonConfig",
      g_variant_new("(s)", "classicui"), nullptr,
      G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000, nullptr, reloadFinished,
      g_object_ref(connection));
  g_object_unref(connection);
}

} // namespace

bool synchronizeFcitxTheme(const Json &palette, std::string *error) {
  const fs::path dataDirectory =
      fs::path(g_get_user_data_dir()) / "fcitx5/themes/lunadash-mellow";
  const std::map<std::string, std::string> roles{
      {"on_surface", "text"},
      {"on_primary", "accentInk"},
      {"primary", "accent"},
      {"surface_container_lowest", "background"},
      {"outline", "border"},
      {"outline_variant", "hairline"},
  };
  for (const auto *name : {"theme.conf", "panel.svg", "highlight.svg"}) {
    std::string contents = readTemplate(name);
    if (contents.empty()) {
      if (error)
        *error = "Missing bundled input-method theme.";
      return false;
    }
    for (const auto &[role, paletteKey] : roles)
      replaceAll(contents, "{{ colors." + role + ".default.hex }}",
                 paletteColor(palette, paletteKey));
    if (contents.find("{{") != std::string::npos) {
      if (error)
        *error = "Unresolved input-method theme color.";
      return false;
    }
    if (!writeFile(dataDirectory / name, contents, error))
      return false;
  }

  const fs::path configPath =
      fs::path(g_get_user_config_dir()) / "fcitx5/conf/classicui.conf";
  std::string config;
  std::error_code filesystemError;
  if (fs::exists(configPath, filesystemError)) {
    if (!readFile(configPath, config, error))
      return false;
    if (!backupOnce(configPath, error))
      return false;
  } else if (filesystemError) {
    if (error)
      *error = "Could not inspect Fcitx appearance settings: " +
               filesystemError.message();
    return false;
  }

  for (const auto *key : {"Theme", "DarkTheme"}) {
    const std::string replacement = std::string(key) + "=lunadash-mellow";
    bool found = false;
    std::string updated;
    std::size_t start = 0;
    while (start <= config.size()) {
      const auto end = config.find('\n', start);
      const auto length = end == std::string::npos ? config.size() - start
                                                    : end - start;
      const std::string_view line(config.data() + start, length);
      if (line.starts_with(std::string(key) + "=")) {
        updated += replacement;
        found = true;
      } else
        updated.append(line);
      if (end == std::string::npos)
        break;
      updated.push_back('\n');
      start = end + 1;
    }
    config = std::move(updated);
    if (!found)
      config.insert(0, replacement + "\n");
  }
  if (!writeFile(configPath, config, error))
    return false;
  reloadFcitx();
  return true;
}

} // namespace LunaDash
