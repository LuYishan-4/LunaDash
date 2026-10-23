#include "service/portal/FileChooserOptions.hpp"

#include <algorithm>

namespace LunaDash {

std::optional<std::vector<std::string>>
portalSaveFileNames(std::vector<std::string> names) {
  if (names.empty())
    return std::nullopt;
  for (auto &name : names) {
    if (!name.empty() && name.back() == '\0')
      name.pop_back();
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string::npos ||
        name.find('\0') != std::string::npos)
      return std::nullopt;
  }
  return names;
}

} // namespace LunaDash
