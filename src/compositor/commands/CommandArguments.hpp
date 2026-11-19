#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace LunaDash {


std::optional<int> parseInteger(std::string_view text);
std::optional<int> textWindowId(const std::string &text);
bool groupWindowIds(const std::string &text, int *window, int *target);

} // namespace LunaDash
