#pragma once

#include <optional>
#include <string>
#include <vector>

namespace LunaDash {

// SaveFiles names are local basenames. Reject an entire request if any entry
// could escape the selected directory or contains an embedded NUL byte.
std::optional<std::vector<std::string>>
portalSaveFileNames(std::vector<std::string> names);

} // namespace LunaDash
