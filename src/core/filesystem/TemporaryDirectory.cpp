#include "core/filesystem/TemporaryDirectory.hpp"

#include <cstdlib>
#include <system_error>

namespace LunaDash {

std::filesystem::path temporaryRoot() {
  const char *directory = std::getenv("TMPDIR");
  if (directory && *directory)
    return directory;
  return "/tmp";
}

TemporaryDirectory::TemporaryDirectory()
    : TemporaryDirectory(temporaryRoot() / "lunadash-XXXXXX") {}

TemporaryDirectory::TemporaryDirectory(const std::filesystem::path &pattern) {
  auto buffer = pattern.string();
  if (const char *created = ::mkdtemp(buffer.data()))
    path_ = created;
}

TemporaryDirectory::~TemporaryDirectory() {
  if (path_.empty())
    return;
  std::error_code error;
  std::filesystem::remove_all(path_, error);
}

std::string TemporaryDirectory::filePath(const std::string &name) const {
  return (path_ / name).string();
}

} // namespace LunaDash
