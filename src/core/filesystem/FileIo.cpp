#include "core/filesystem/FileIo.hpp"

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>

namespace LunaDash {

std::optional<std::string> readFile(const std::filesystem::path &path,
                                    std::size_t maximumBytes) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return std::nullopt;
  std::string contents;
  char buffer[16384];
  while (input) {
    input.read(buffer, sizeof buffer);
    const auto count = static_cast<std::size_t>(input.gcount());
    if (contents.size() + count > maximumBytes)
      return std::nullopt;
    contents.append(buffer, count);
  }
  if (input.bad())
    return std::nullopt;
  return contents;
}

bool createDirectories(const std::filesystem::path &path, std::string *error) {
  std::error_code code;
  std::filesystem::create_directories(path, code);
  if (code && !std::filesystem::is_directory(path)) {
    if (error)
      *error = "Could not create " + path.string() + ": " + code.message();
    return false;
  }
  return true;
}

bool writeFileAtomically(const std::filesystem::path &path,
                         std::string_view contents, std::string *error,
                         std::filesystem::perms permissions) {
  const auto parent = path.parent_path();
  if (!parent.empty() && !createDirectories(parent, error))
    return false;
  auto pattern = (parent.empty() ? std::filesystem::path(".") : parent) /
                 ("." + path.filename().string() + ".XXXXXX");
  auto buffer = pattern.string();
  const int descriptor = ::mkstemp(buffer.data());
  if (descriptor < 0) {
    if (error)
      *error = "Could not create a temporary file for " + path.string() +
               ": " + std::strerror(errno);
    return false;
  }
  bool success = true;
  const char *data = contents.data();
  std::size_t remaining = contents.size();
  while (success && remaining > 0) {
    const auto written = ::write(descriptor, data, remaining);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0) {
      success = false;
      break;
    }
    data += written;
    remaining -= static_cast<std::size_t>(written);
  }
  success = success && ::fchmod(descriptor, static_cast<mode_t>(permissions)) == 0;
  success = success && ::fsync(descriptor) == 0;
  success = (::close(descriptor) == 0) && success;
  success = success && ::rename(buffer.c_str(), path.c_str()) == 0;
  if (!success) {
    const auto reason = std::strerror(errno);
    ::unlink(buffer.c_str());
    if (error)
      *error = "Could not write " + path.string() + ": " + reason;
  }
  return success;
}

std::string environmentValue(const char *name) {
  const char *value = std::getenv(name);
  return value ? std::string(value) : std::string();
}

int environmentInt(const char *name, int fallback) {
  const auto value = environmentValue(name);
  if (value.empty())
    return fallback;
  int result = 0;
  const auto *begin = value.data();
  const auto *end = value.data() + value.size();
  const auto parsed = std::from_chars(begin, end, result);
  return parsed.ec == std::errc() && parsed.ptr == end ? result : fallback;
}

} // namespace LunaDash
