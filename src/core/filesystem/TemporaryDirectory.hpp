#pragma once

#include <filesystem>
#include <string>

namespace LunaDash {

class TemporaryDirectory final {
public:
  // Creates <TMPDIR>/lunadash-XXXXXX.
  TemporaryDirectory();
  // Creates a directory from a mkdtemp pattern ending in XXXXXX.
  explicit TemporaryDirectory(const std::filesystem::path &pattern);
  ~TemporaryDirectory();
  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  bool isValid() const noexcept { return !path_.empty(); }
  const std::filesystem::path &path() const noexcept { return path_; }
  std::string filePath(const std::string &name) const;

private:
  std::filesystem::path path_;
};

std::filesystem::path temporaryRoot();

} // namespace LunaDash
