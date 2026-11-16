#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace LunaDash {

// Parsers for the plain-text values of control requests.

std::optional<int> parseInteger(std::string_view text);
// A positive window ID, surrounding whitespace allowed.
std::optional<int> textWindowId(const std::string &text);
// {"window": N, "target": M} with distinct positive IDs.
bool groupWindowIds(const std::string &text, int *window, int *target);

} // namespace LunaDash
