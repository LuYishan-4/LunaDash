#include "desktop/theme/ThemeSync.hpp"

#include "desktop/theme/FcitxTheme.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <csignal>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace LunaDash {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

struct ChildProcess {
  GPid pid = 0;
  guint timeout = 0;
};

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
      *error = "Theme integration requires a readable configuration: " +
               filesystemError.message();
    return false;
  }
  if (size > 1024 * 1024) {
    if (error)
      *error = "Theme integration requires a configuration below 1 MiB: " +
               path.string();
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (error)
      *error = "Theme integration requires a readable configuration: " +
               path.string();
    return false;
  }
  contents.assign(std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>());
  if (input.bad()) {
    if (error)
      *error = "Could not read theme configuration: " + path.string();
    return false;
  }
  return true;
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

bool includeOnce(const fs::path &path, const std::string &line,
                 std::string *error) {
  std::string contents;
  std::error_code filesystemError;
  const bool exists = fs::exists(path, filesystemError);
  if (filesystemError) {
    if (error)
      *error = "Could not inspect configuration: " +
               filesystemError.message();
    return false;
  }
  if (exists) {
    if (!readFile(path, contents, error))
      return false;
    if (contents.find(line) != std::string::npos)
      return true;
    if (!backupOnce(path, error))
      return false;
  }
  const auto updated = line.starts_with("@import")
                           ? line + "\n" + contents
                           : contents + "\n" + line + "\n";
  return writeFile(path, updated, error);
}

bool setIniValue(const fs::path &path, const std::string &section,
                 const std::string &key, const std::string &value,
                 std::string *error) {
  std::string contents;
  if (!readFile(path, contents, error))
    return false;
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < contents.size()) {
    const auto end = contents.find('\n', start);
    auto line = contents.substr(start, end == std::string::npos
                                           ? contents.size() - start
                                           : end - start);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    lines.push_back(std::move(line));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }

  const std::string sectionHeader = "[" + section + "]";
  const std::string setting = key + "=" + value;
  std::size_t sectionStart = lines.size();
  std::size_t sectionEnd = lines.size();
  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (lines[index] == sectionHeader) {
      sectionStart = index;
      sectionEnd = lines.size();
      for (std::size_t next = index + 1; next < lines.size(); ++next)
        if (lines[next].starts_with('[')) {
          sectionEnd = next;
          break;
        }
      break;
    }
  }
  if (sectionStart == lines.size()) {
    if (!lines.empty() && !lines.back().empty())
      lines.emplace_back();
    lines.push_back(sectionHeader);
    lines.push_back(setting);
  } else {
    bool replaced = false;
    for (std::size_t index = sectionStart + 1; index < sectionEnd; ++index) {
      if (lines[index].starts_with(key + "=")) {
        lines[index] = setting;
        replaced = true;
      }
    }
    if (!replaced)
      lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd),
                   setting);
  }

  std::string updated;
  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (index)
      updated.push_back('\n');
    updated += lines[index];
  }
  updated.push_back('\n');
  return writeFile(path, updated, error);
}

std::string color(const Json &palette, std::string_view key) {
  const auto found = palette.find(key);
  return found != palette.end() && found->is_string()
             ? found->get<std::string>()
             : std::string{};
}

gboolean killChild(gpointer data) {
  auto *child = static_cast<ChildProcess *>(data);
  child->timeout = 0;
  if (child->pid > 0)
    ::kill(static_cast<pid_t>(child->pid), SIGKILL);
  return G_SOURCE_REMOVE;
}

void reapChild(GPid pid, gint, gpointer data) {
  auto *child = static_cast<ChildProcess *>(data);
  if (child->timeout)
    g_source_remove(child->timeout);
  g_spawn_close_pid(pid);
  delete child;
}

void setColorScheme(bool dark) {
  gchar *program = g_find_program_in_path("gsettings");
  if (!program)
    return;
  gchar *arguments[] = {
      program,
      const_cast<gchar *>("set"),
      const_cast<gchar *>("org.gnome.desktop.interface"),
      const_cast<gchar *>("color-scheme"),
      const_cast<gchar *>(dark ? "prefer-dark" : "prefer-light"),
      nullptr,
  };
  GPid pid = 0;
  GError *error = nullptr;
  const auto flags = static_cast<GSpawnFlags>(
      G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
      G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL);
  const gboolean started = g_spawn_async(nullptr, arguments, nullptr, flags,
                                         nullptr, nullptr, &pid, &error);
  g_free(program);
  if (!started) {
    g_clear_error(&error);
    return;
  }
  auto *child = new ChildProcess{pid, 0};
  child->timeout = g_timeout_add(3000, killChild, child);
  g_child_watch_add(pid, reapChild, child);
}

} // namespace

Json synchronizeApplicationTheme(const Json &preferences,
                                 const Json &palette) {
  static std::string previous;
  static Json result;
  const bool enabled = preferences.value("syncApplicationThemes", false);
  const bool fcitx = preferences.value("syncFcitxTheme", false);
  const auto fingerprint = palette.dump() + (enabled ? "1" : "0") +
                           (fcitx ? "1" : "0");
  if (fingerprint == previous)
    return result;
  previous = fingerprint;

  const fs::path config = g_get_user_config_dir();
  const auto directory = config / "lunadash/theme";
  result = {{"enabled", enabled},
            {"directory", directory.string()},
            {"error", ""}};
  std::string fcitxError;
  if (fcitx && !synchronizeFcitxTheme(palette, &fcitxError))
    result["error"] = fcitxError;

  // This system preference is independent of the optional GTK and Kitty
  // theme files, so publish it even when application theme sync is disabled.
  setColorScheme(palette.value("dark", false));
  if (!enabled)
    return result;

  const auto background = color(palette, "background");
  const auto foreground = color(palette, "text");
  const auto surface = color(palette, "surface");
  const auto accent = color(palette, "accent");
  const auto accentInk = color(palette, "accentInk");
  const std::string gtk =
      "@define-color theme_bg_color " + background + ";\n" +
      "@define-color theme_fg_color " + foreground + ";\n" +
      "@define-color theme_base_color " + surface + ";\n" +
      "@define-color theme_selected_bg_color " + accent + ";\n" +
      "@define-color theme_selected_fg_color " + accentInk + ";\n" +
      "@define-color accent_color " + accent + ";\n" +
      "@define-color accent_bg_color " + accent + ";\n" +
      "@define-color accent_fg_color " + accentInk + ";\n" +
      "@define-color window_bg_color " + background + ";\n" +
      "@define-color window_fg_color " + foreground + ";\n" +
      "@define-color view_bg_color " + surface + ";\n" +
      "@define-color view_fg_color " + foreground + ";\n";
  const std::string kitty =
      "# Generated by LunaDash. Include from kitty.conf.\nbackground " +
      background + "\nforeground " + foreground + "\nselection_background " +
      accent + "\nselection_foreground " + accentInk + "\ncursor " + accent +
      "\nurl_color " + accent + "\n";
  std::string error;
  if (!writeFile(directory / "gtk.css", gtk, &error) ||
      !writeFile(directory / "kitty.conf", kitty, &error)) {
    result["error"] = error;
    return result;
  }

  GError *uriError = nullptr;
  gchar *gtkUri = g_filename_to_uri((directory / "gtk.css").c_str(), nullptr,
                                    &uriError);
  if (!gtkUri) {
    result["error"] = uriError ? uriError->message
                               : "Could not create GTK theme URI.";
    g_clear_error(&uriError);
    return result;
  }
  const std::string import = "@import url(\"" + std::string(gtkUri) + "\");";
  g_free(gtkUri);

  for (const auto *version : {"gtk-3.0", "gtk-4.0"}) {
    const auto base = config / version;
    if (!includeOnce(base / "gtk.css", import, &error))
      break;
    if (!setIniValue(base / "settings.ini", "Settings",
                     "gtk-application-prefer-dark-theme",
                     palette.value("dark", false) ? "1" : "0", &error))
      break;
  }
  const auto kittyConfig = config / "kitty/kitty.conf";
  std::error_code filesystemError;
  if (error.empty() && fs::exists(kittyConfig, filesystemError) &&
      !includeOnce(kittyConfig,
                   "include ../lunadash/theme/kitty.conf", &error)) {
    // The existing terminal configuration stays usable if the optional include
    // cannot be added; return the failure for the settings UI to display.
  }
  if (filesystemError && error.empty())
    error = "Could not inspect Kitty configuration: " +
            filesystemError.message();
  result["error"] = error.empty() ? fcitxError : error;
  return result;
}

} // namespace LunaDash
