#include "compositor/commands/CommandArguments.hpp"

#include "core/json/JsonAccess.hpp"
#include "core/text/Text.hpp"

#include <charconv>
#include <climits>
#include <cstdint>

namespace LunaDash {

std::optional<int> parseInteger(std::string_view text) {
  int value = 0;
  const auto *begin = text.data();
  const auto *end = text.data() + text.size();
  const auto result = std::from_chars(begin, end, value);
  if (text.empty() || result.ec != std::errc() || result.ptr != end)
    return std::nullopt;
  return value;
}

std::optional<int> textWindowId(const std::string &text) {
  const auto id = parseInteger(trimmed(text));
  if (!id || *id <= 0)
    return std::nullopt;
  return id;
}

bool groupWindowIds(const std::string &text, int *window, int *target) {
  const auto object = parseJsonObject(text);
  if (!object.is_object())
    return false;
  const auto left = jsonValue(object, "window");
  const auto right = jsonValue(object, "target");
  if (!left.is_number() || !right.is_number())
    return false;
  const std::int64_t a = jsonAsInteger(left);
  const std::int64_t b = jsonAsInteger(right);
  if (a <= 0 || b <= 0 || a > INT_MAX || b > INT_MAX || a == b)
    return false;
  *window = static_cast<int>(a);
  *target = static_cast<int>(b);
  return true;
}

} // namespace LunaDash
