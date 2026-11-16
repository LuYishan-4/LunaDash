#include "compositor/session/ShellLocation.hpp"

#include "core/process/ExecutableLookup.hpp"

#include <glib.h>

#include <filesystem>
#include <system_error>
#include <vector>

namespace LunaDash {
namespace {

std::string locateDataFile(const std::string &relativePath) {
  const auto relative = std::filesystem::path(relativePath);
  const auto findIn = [&relative](const char *base) -> std::string {
    if (!base)
      return {};
    const auto candidate = std::filesystem::path(base) / relative;
    std::error_code error;
    if (!std::filesystem::is_regular_file(candidate, error) || error)
      return {};
    return candidate.string();
  };
  if (auto path = findIn(g_get_user_data_dir()); !path.empty())
    return path;
  for (const char *const *directory = g_get_system_data_dirs(); *directory;
       ++directory)
    if (auto path = findIn(*directory); !path.empty())
      return path;
  return {};
}

} // namespace

std::string locateSessionShell() {
  std::vector<std::filesystem::path> candidates{
      (std::filesystem::path(executableDirectory()) / "../share/lunadash/shell")
          .lexically_normal()};
  if (*LUDASH_SHELL_SOURCE_DIR)
    candidates.emplace_back(LUDASH_SHELL_SOURCE_DIR);
  for (const char *directory : {"lunadash/shell", "ludash/shell"}) {
    const auto found = locateDataFile(std::string(directory) + "/shell.slint");
    if (!found.empty())
      candidates.push_back(std::filesystem::path(found).parent_path());
  }
  for (const auto &directory : candidates)
    if (fileExists((directory / "shell.slint").string()))
      return directory.string();
  return {};
}

std::string findSlintLu() {
  const auto directory = std::filesystem::path(executableDirectory());
  for (const auto &candidate :
       {(directory / "../libexec/lunadash/slintlu").lexically_normal(),
        directory / "slintlu/release/slintlu"})
    if (fileIsExecutable(candidate.string()))
      return candidate.string();
  if (auto path = findExecutable("slintlu"); !path.empty())
    return path;
  std::vector<std::filesystem::path> candidates;
  if (const char *home = g_get_home_dir()) {
    candidates.push_back(std::filesystem::path(home) / ".cargo/bin/slintlu");
    candidates.push_back(std::filesystem::path(home) / ".local/bin/slintlu");
  }
  for (const auto &candidate : candidates)
    if (fileIsExecutable(candidate.string()))
      return candidate.string();
  return {};
}

std::string bundledExecutable(const std::string &name) {
  const auto directory = executableDirectory();
  const auto executable = directory + "/lunadash-" + name;
  if (fileExists(executable))
    return executable;
  return directory + "/ludash-" + name;
}

std::string findClipboardHelper() {
  if (auto helper = findExecutable("lunadash-clipboard-history"); !helper.empty())
    return helper;
  const std::string source =
      std::string(LUDASH_SCRIPT_SOURCE_DIR) + "/lunadash-clipboard-history";
  return fileIsExecutable(source) ? source : std::string{};
}

} // namespace LunaDash
