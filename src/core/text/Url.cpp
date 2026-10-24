#include "core/text/Url.hpp"

namespace LunaDash {
namespace {
bool unreserved(unsigned char character) {
  return (character >= 'A' && character <= 'Z') ||
         (character >= 'a' && character <= 'z') ||
         (character >= '0' && character <= '9') || character == '-' ||
         character == '.' || character == '_' || character == '~';
}

int hexValue(char character) {
  if (character >= '0' && character <= '9')
    return character - '0';
  if (character >= 'a' && character <= 'f')
    return character - 'a' + 10;
  if (character >= 'A' && character <= 'F')
    return character - 'A' + 10;
  return -1;
}
} // namespace

std::string percentEncode(std::string_view text, std::string_view keep) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(text.size());
  for (const char value : text) {
    const auto character = static_cast<unsigned char>(value);
    if (unreserved(character) || keep.find(value) != std::string_view::npos) {
      result.push_back(value);
      continue;
    }
    result.push_back('%');
    result.push_back(digits[character >> 4]);
    result.push_back(digits[character & 15]);
  }
  return result;
}

std::string percentDecode(std::string_view text) {
  std::string result;
  result.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '%' && index + 2 < text.size()) {
      const int high = hexValue(text[index + 1]);
      const int low = hexValue(text[index + 2]);
      if (high >= 0 && low >= 0) {
        result.push_back(static_cast<char>(high * 16 + low));
        index += 2;
        continue;
      }
    }
    result.push_back(text[index]);
  }
  return result;
}

std::string fileUrl(std::string_view path) {
  return "file://" + percentEncode(path, "/");
}

std::string localPathFromFileUrl(std::string_view url) {
  constexpr std::string_view scheme = "file://";
  if (url.substr(0, scheme.size()) != scheme)
    return {};
  auto rest = url.substr(scheme.size());
  if (rest.substr(0, 9) == "localhost")
    rest.remove_prefix(9);
  if (rest.empty() || rest.front() != '/')
    return {};
  const auto end = rest.find_first_of("?#");
  return percentDecode(rest.substr(0, end));
}

std::string urlScheme(std::string_view url) {
  const auto colon = url.find(':');
  if (colon == std::string_view::npos || colon == 0)
    return {};
  std::string scheme;
  for (std::size_t index = 0; index < colon; ++index) {
    const char character = url[index];
    const bool letter = (character >= 'a' && character <= 'z') ||
                        (character >= 'A' && character <= 'Z');
    const bool other = (character >= '0' && character <= '9') ||
                       character == '+' || character == '-' || character == '.';
    if (!letter && (index == 0 || !other))
      return {};
    scheme.push_back(character >= 'A' && character <= 'Z'
                         ? static_cast<char>(character - 'A' + 'a')
                         : character);
  }
  return scheme;
}

bool isHttpsUrl(std::string_view url) {
  constexpr std::string_view prefix = "https://";
  if (url.size() <= prefix.size() || urlScheme(url) != "https" ||
      url.substr(5, 3) != "://")
    return false;
  for (const char value : url) {
    const auto character = static_cast<unsigned char>(value);
    if (character <= 0x20 || character == 0x7F)
      return false;
  }
  const auto authority = url.substr(prefix.size(),
                                    url.find_first_of("/?#", prefix.size()) -
                                        prefix.size());
  const auto at = authority.rfind('@');
  const auto host = at == std::string_view::npos ? authority
                                                 : authority.substr(at + 1);
  return !host.empty() && host.front() != ':';
}

} // namespace LunaDash
