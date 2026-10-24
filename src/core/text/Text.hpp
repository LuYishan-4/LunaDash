#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

std::string asciiLower(std::string_view text);
std::string asciiUpper(std::string_view text);
bool containsText(std::string_view text, std::string_view needle);
bool containsTextIgnoringCase(std::string_view text, std::string_view needle);
bool equalsIgnoringCase(std::string_view a, std::string_view b);
std::string trimmed(std::string_view text);
// Splits on a separator. Empty parts are kept unless skipEmpty is set.
std::vector<std::string> splitText(std::string_view text, char separator,
                                   bool skipEmpty = false);
std::vector<std::string> splitWhitespace(std::string_view text);
std::string joinText(const std::vector<std::string> &parts,
                     std::string_view separator);
std::string replaceAll(std::string text, std::string_view from,
                       std::string_view to);
bool validUtf8(std::string_view text);
// Copy of text with each invalid UTF-8 sequence replaced by U+FFFD.
std::string sanitizedUtf8(std::string_view text);

} // namespace LunaDash
