#include "core/text/Text.hpp"

#include <algorithm>
#include <cstdint>

namespace LunaDash {
namespace {
char lowerAscii(char character) {
  return character >= 'A' && character <= 'Z'
             ? static_cast<char>(character - 'A' + 'a')
             : character;
}
bool space(char character) {
  return character == ' ' || character == '\t' || character == '\n' ||
         character == '\r' || character == '\f' || character == '\v';
}
} // namespace

std::string asciiLower(std::string_view text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(), lowerAscii);
  return result;
}

std::string asciiUpper(std::string_view text) {
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(), [](char c) {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
  });
  return result;
}

bool containsText(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

bool containsTextIgnoringCase(std::string_view text, std::string_view needle) {
  return asciiLower(text).find(asciiLower(needle)) != std::string::npos;
}

bool equalsIgnoringCase(std::string_view a, std::string_view b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return lowerAscii(x) == lowerAscii(y);
         });
}

std::string trimmed(std::string_view text) {
  while (!text.empty() && space(text.front()))
    text.remove_prefix(1);
  while (!text.empty() && space(text.back()))
    text.remove_suffix(1);
  return std::string(text);
}

std::vector<std::string> splitText(std::string_view text, char separator,
                                   bool skipEmpty) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (true) {
    const auto end = text.find(separator, start);
    const auto part = text.substr(start, end == std::string_view::npos
                                             ? std::string_view::npos
                                             : end - start);
    if (!skipEmpty || !part.empty())
      parts.emplace_back(part);
    if (end == std::string_view::npos)
      break;
    start = end + 1;
  }
  return parts;
}

std::vector<std::string> splitWhitespace(std::string_view text) {
  std::vector<std::string> parts;
  std::size_t index = 0;
  while (index < text.size()) {
    while (index < text.size() && space(text[index]))
      ++index;
    const auto start = index;
    while (index < text.size() && !space(text[index]))
      ++index;
    if (index > start)
      parts.emplace_back(text.substr(start, index - start));
  }
  return parts;
}

std::string joinText(const std::vector<std::string> &parts,
                     std::string_view separator) {
  std::string result;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index)
      result += separator;
    result += parts[index];
  }
  return result;
}

std::string replaceAll(std::string text, std::string_view from,
                       std::string_view to) {
  if (from.empty())
    return text;
  std::size_t position = 0;
  while ((position = text.find(from, position)) != std::string::npos) {
    text.replace(position, from.size(), to);
    position += to.size();
  }
  return text;
}

namespace {
// Length of the valid UTF-8 sequence at index, or 0 when it is invalid.
std::size_t utf8SequenceLength(std::string_view text, std::size_t index) {
  const auto lead = static_cast<unsigned char>(text[index]);
  std::size_t length = 0;
  std::uint32_t codePoint = 0;
  if (lead < 0x80)
    return 1;
  if ((lead & 0xE0) == 0xC0) {
    length = 2;
    codePoint = lead & 0x1F;
  } else if ((lead & 0xF0) == 0xE0) {
    length = 3;
    codePoint = lead & 0x0F;
  } else if ((lead & 0xF8) == 0xF0) {
    length = 4;
    codePoint = lead & 0x07;
  } else {
    return 0;
  }
  if (index + length > text.size())
    return 0;
  for (std::size_t offset = 1; offset < length; ++offset) {
    const auto next = static_cast<unsigned char>(text[index + offset]);
    if ((next & 0xC0) != 0x80)
      return 0;
    codePoint = (codePoint << 6) | (next & 0x3F);
  }
  if ((length == 2 && codePoint < 0x80) || (length == 3 && codePoint < 0x800) ||
      (length == 4 && (codePoint < 0x10000 || codePoint > 0x10FFFF)) ||
      (codePoint >= 0xD800 && codePoint <= 0xDFFF))
    return 0;
  return length;
}
} // namespace

std::string sanitizedUtf8(std::string_view text) {
  std::string result;
  result.reserve(text.size());
  std::size_t index = 0;
  while (index < text.size()) {
    const auto length = utf8SequenceLength(text, index);
    if (length == 0) {
      result += "\xEF\xBF\xBD";
      ++index;
      continue;
    }
    result.append(text.substr(index, length));
    index += length;
  }
  return result;
}

bool validUtf8(std::string_view text) {
  std::size_t index = 0;
  while (index < text.size()) {
    const auto length = utf8SequenceLength(text, index);
    if (length == 0)
      return false;
    index += length;
  }
  return true;
}

} // namespace LunaDash
