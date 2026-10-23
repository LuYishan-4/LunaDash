#include "shell/modules/ShellModules.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace LunaDash {
namespace {

constexpr std::size_t maximumDocumentBytes = 16 * 1024;

bool writeAtomically(const std::string &path, std::string_view bytes) {
  struct stat info {};
  const int statResult = ::lstat(path.c_str(), &info);
  if (statResult == 0 && S_ISLNK(info.st_mode))
    return false;
  if (statResult != 0 && errno != ENOENT)
    return false;

  std::string temporary = path + ".tmp.XXXXXX";
  std::vector<char> name(temporary.begin(), temporary.end());
  name.push_back('\0');
  const int fd = ::mkstemp(name.data());
  if (fd < 0)
    return false;
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
  if (!ok)
    ::unlink(temporaryPath.c_str());
  return ok;
}

std::string moduleConfigPath() {
  std::filesystem::path base(g_get_user_config_dir());
  base /= "LuDash";
  return (base / "shell-modules.json").string();
}

} // namespace

ShellModules::ShellModules() : path_(moduleConfigPath()),
                               document_(defaultModuleDocument()) {
  std::error_code error;
  const auto directory = std::filesystem::path(path_).parent_path();
  std::filesystem::create_directories(directory, error);
  if (error) {
    status_ = "Could not create module configuration directory.";
  }

  auto *watchDirectory = g_file_new_for_path(directory.c_str());
  GError *monitorError = nullptr;
  monitor_ = g_file_monitor_directory(watchDirectory,
                                     G_FILE_MONITOR_WATCH_MOVES, nullptr,
                                     &monitorError);
  g_object_unref(watchDirectory);
  if (monitorError) {
    if (status_.empty())
      status_ = "Could not monitor module configuration changes.";
    g_error_free(monitorError);
  }
  if (monitor_) {
    g_signal_connect(
        monitor_, "changed",
        G_CALLBACK(+[](GFileMonitor *, GFile *file, GFile *other,
                       GFileMonitorEvent, gpointer data) {
          auto *self = static_cast<ShellModules *>(data);
          const auto matches = [self](GFile *candidate) {
            if (!candidate)
              return false;
            char *filename = g_file_get_path(candidate);
            const bool match = filename && self->path_ == filename;
            g_free(filename);
            return match;
          };
          if (!matches(file) && !matches(other))
            return;
          if (self->debounceSource_)
            g_source_remove(self->debounceSource_);
          self->debounceSource_ = g_timeout_add_full(
              G_PRIORITY_DEFAULT, 100,
              +[](gpointer timeoutData) -> gboolean {
                auto *owner = static_cast<ShellModules *>(timeoutData);
                owner->debounceSource_ = 0;
                owner->reload();
                return G_SOURCE_REMOVE;
              },
              self, nullptr);
        }),
        this);
  }

  if (!std::filesystem::exists(path_)) {
    std::string saveError;
    if (!apply(document_.dump(2), &saveError))
      status_ = std::move(saveError);
  }
  reload();
}

ShellModules::~ShellModules() {
  if (debounceSource_)
    g_source_remove(debounceSource_);
  if (monitor_) {
    g_file_monitor_cancel(monitor_);
    g_object_unref(monitor_);
    monitor_ = nullptr;
  }
}

bool ShellModules::validate(std::string_view text, std::string *error) {
  ShellModuleJson document;
  const bool valid = validateModuleDocument(text, &document, error);
  status_ = valid ? "Module JSON is valid."
                  : (error && !error->empty() ? *error : "Invalid module JSON.");
  return valid;
}

bool ShellModules::apply(std::string_view text, std::string *error) {
  ShellModuleJson document;
  if (!validateModuleDocument(text, &document, error))
    return false;
  struct stat info {};
  const int statResult = ::lstat(path_.c_str(), &info);
  if (statResult == 0 && S_ISLNK(info.st_mode)) {
    if (error)
      *error = "Refusing to overwrite a symlinked module configuration.";
    return false;
  }
  if (statResult != 0 && errno != ENOENT) {
    if (error)
      *error = "Could not save module configuration atomically.";
    return false;
  }
  if (!writeAtomically(path_, document.dump(2))) {
    if (error)
      *error = "Could not save module configuration atomically.";
    return false;
  }
  status_ = "Module configuration saved.";
  reload();
  return true;
}

bool ShellModules::reset(std::string *error) {
  if (!apply(defaultModuleDocument().dump(), error))
    return false;
  errors_ = ShellModuleJson::object();
  status_ = "Built-in modules restored. Legacy files were preserved.";
  return true;
}

void ShellModules::reload() {
  ShellModuleJson next = defaultModuleDocument();
  std::string error;
  std::error_code fileError;
  if (std::filesystem::exists(path_, fileError) && !fileError) {
    const auto size = std::filesystem::file_size(path_, fileError);
    if (fileError || size > maximumDocumentBytes) {
      status_ = "Module JSON must be at most 16 KiB.";
      return;
    }
    std::ifstream file(path_, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (!file || !validateModuleDocument(text, &next, &error)) {
      status_ = error.empty()
                    ? "Could not read module configuration; previous settings are retained."
                    : error;
      return;
    }
  } else if (fileError) {
    status_ = "Could not read module configuration; previous settings are retained.";
    return;
  }
  if (document_ != next) {
    document_ = std::move(next);
    revision_ = (revision_ + 1) % 1000000;
    errors_ = ShellModuleJson::object();
    if (changedCallback_)
      changedCallback_();
  }
}

ShellModuleJson ShellModules::snapshot() const {
  return {{"document", document_},
          {"modules", document_.value("modules", ShellModuleJson::object())},
          {"descriptors", shellModuleDescriptors()},
          {"path", path_},
          {"revision", revision_},
          {"status", status_},
          {"errors", errors_}};
}

void ShellModules::reportError(std::string_view id, std::string_view error) {
  const auto ids = shellModuleIds();
  if (std::find(ids.begin(), ids.end(), id) != ids.end())
    errors_[std::string(id)] = std::string(error.substr(0, 512));
}

int ShellModules::panelExtent(int fallbackHeight) const {
  const auto panel = document_.at("modules").value("panel", ShellModuleJson::object());
  if (!panel.value("enabled", true))
    return 0;
  const auto style = panel.value("style", ShellModuleJson::object());
  const int height = style.value("height", 0);
  return std::max(24, height ? height : fallbackHeight) +
         2 * style.value("margin", 0);
}

std::string ShellModules::panelEdge() const {
  const auto panel = document_.at("modules").value("panel", ShellModuleJson::object());
  const auto style = panel.value("style", ShellModuleJson::object());
  const auto edge = style.value("edge", std::string("top"));
  return edge == "top" || edge == "bottom" || edge == "left" || edge == "right"
             ? edge
             : "top";
}

bool ShellModules::panelAtBottom() const { return panelEdge() == "bottom"; }

void ShellModules::setChangedCallback(std::function<void()> callback) {
  changedCallback_ = std::move(callback);
}

} // namespace LunaDash
