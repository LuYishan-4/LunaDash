#include "core/process/ExecutableLookup.hpp"

#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace LunaDash {
namespace {

bool executableFile(const std::filesystem::path &path) {
  struct stat status{};
  return ::stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
         ::access(path.c_str(), X_OK) == 0;
}

} // namespace

std::string findExecutable(std::string_view program,
                           std::string_view searchPath) {
  if (program.empty())
    return {};

  const std::filesystem::path requested{std::string(program)};
  if (program.find('/') != std::string_view::npos)
    return executableFile(requested) ? requested.string() : std::string{};

  std::size_t start = 0;
  while (start <= searchPath.size()) {
    const auto separator = searchPath.find(':', start);
    const auto directory = searchPath.substr(
        start, separator == std::string_view::npos ? searchPath.size() - start
                                                   : separator - start);
    const auto candidate =
        (directory.empty() ? std::filesystem::path(".")
                           : std::filesystem::path(std::string(directory))) /
        requested;
    if (executableFile(candidate))
      return candidate.string();
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }
  return {};
}

std::string findExecutable(std::string_view program) {
  const char *path = std::getenv("PATH");
  return findExecutable(
      program, path ? std::string_view(path)
                    : std::string_view("/usr/local/bin:/usr/bin:/bin"));
}

std::string executableDirectory() {
  std::error_code error;
  const auto executable =
      std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? std::string{} : executable.parent_path().string();
}

bool fileExists(const std::string &path) {
  std::error_code error;
  const bool exists =
      std::filesystem::exists(std::filesystem::path(path), error);
  return exists && !error;
}

bool fileIsExecutable(const std::string &path) {
  return executableFile(std::filesystem::path(path));
}

} // namespace LunaDash
