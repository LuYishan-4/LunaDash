#include "desktop/browser/Browser.hpp"

#include "core/process/ExecutableLookup.hpp"

#include <array>

namespace LunaDash {

std::vector<std::string> Browser::defaultCommand() {
  constexpr std::array candidates{"google-chrome-stable", "google-chrome",
                                  "chromium", "chromium-browser"};
  for (const auto candidate : candidates)
    if (!findExecutable(candidate).empty())
      return {candidate};
  return {};
}

} // namespace LunaDash
