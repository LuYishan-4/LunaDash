#include "desktop/browser/Browser.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace LunaDash {

std::vector<std::string> Browser::defaultCommand() {
  constexpr std::array candidates{"google-chrome-stable", "google-chrome",
                                  "chromium", "chromium-browser"};
  const char *pathValue = std::getenv("PATH");
  if (!pathValue)
    return {};
  const std::string path(pathValue);
  for (const auto candidate : candidates) {
    std::size_t begin = 0;
    while (begin <= path.size()) {
      const auto end = path.find(':', begin);
      const auto component = path.substr(
          begin, end == std::string::npos ? std::string::npos : end - begin);
      const std::filesystem::path directory =
          component.empty() ? std::filesystem::current_path()
                            : std::filesystem::path(component);
      if (::access((directory / candidate).c_str(), X_OK) == 0)
        return {candidate};
      if (end == std::string::npos)
        break;
      begin = end + 1;
    }
  }
  return {};
}

} // namespace LunaDash
