#include "core/process/ExecutableLookup.hpp"

#include <glib.h>

#include <filesystem>
#include <system_error>

namespace LunaDash {

std::string findExecutable(const std::string &name) {
  gchar *path = g_find_program_in_path(name.c_str());
  const std::string result = path ? std::string(path) : std::string{};
  g_free(path);
  return result;
}

std::string executableDirectory() {
  std::error_code error;
  const auto executable =
      std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? std::string{} : executable.parent_path().string();
}

bool fileExists(const std::string &path) {
  std::error_code error;
  const bool exists = std::filesystem::exists(std::filesystem::path(path), error);
  return exists && !error;
}

bool fileIsExecutable(const std::string &path) {
  std::error_code error;
  const auto permissions =
      std::filesystem::status(std::filesystem::path(path), error).permissions();
  using Permissions = std::filesystem::perms;
  return !error &&
         (permissions & (Permissions::owner_exec | Permissions::group_exec |
                         Permissions::others_exec)) != Permissions::none;
}

} // namespace LunaDash
