#include "core/json/JsonAccess.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

namespace LunaDash {

const Json *jsonMember(const Json &object, std::string_view key) {
  if (!object.is_object())
    return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}

bool jsonContains(const Json &object, std::string_view key) {
  return jsonMember(object, key) != nullptr;
}

bool jsonIsIntegral(const Json &value) {
  if (value.is_number_integer())
    return true;
  if (!value.is_number_float())
    return false;
  const auto number = value.get<double>();
  return std::isfinite(number) && std::trunc(number) == number;
}

std::string jsonAsString(const Json &value, std::string fallback) {
  return value.is_string() ? value.get<std::string>() : std::move(fallback);
}

std::int64_t jsonAsInteger(const Json &value, std::int64_t fallback) {
  if (value.is_number_unsigned()) {
    const auto number = value.get<std::uint64_t>();
    return number <= static_cast<std::uint64_t>(
                         std::numeric_limits<std::int64_t>::max())
               ? static_cast<std::int64_t>(number)
               : fallback;
  }
  if (value.is_number_integer())
    return value.get<std::int64_t>();
  if (value.is_number_float()) {
    const auto number = value.get<double>();
    if (!std::isfinite(number) || std::trunc(number) != number ||
        number < -9007199254740992.0 || number > 9007199254740992.0)
      return fallback;
    return static_cast<std::int64_t>(number);
  }
  return fallback;
}

int jsonAsInt(const Json &value, int fallback) {
  if (!value.is_number())
    return fallback;
  const auto sentinel = std::numeric_limits<std::int64_t>::min();
  const auto number = jsonAsInteger(value, sentinel);
  if (number == sentinel || number < std::numeric_limits<int>::min() ||
      number > std::numeric_limits<int>::max())
    return fallback;
  return static_cast<int>(number);
}

double jsonAsDouble(const Json &value, double fallback) {
  return value.is_number() ? value.get<double>() : fallback;
}

bool jsonAsBool(const Json &value, bool fallback) {
  return value.is_boolean() ? value.get<bool>() : fallback;
}

Json jsonAsObject(const Json &value) {
  return value.is_object() ? value : Json::object();
}

Json jsonAsArray(const Json &value) {
  return value.is_array() ? value : Json::array();
}

namespace {
const Json &memberOrNull(const Json &object, std::string_view key) {
  static const Json null;
  const auto *member = jsonMember(object, key);
  return member ? *member : null;
}
} // namespace

std::string jsonString(const Json &object, std::string_view key,
                       std::string fallback) {
  return jsonAsString(memberOrNull(object, key), std::move(fallback));
}
int jsonInt(const Json &object, std::string_view key, int fallback) {
  return jsonAsInt(memberOrNull(object, key), fallback);
}
std::int64_t jsonInteger(const Json &object, std::string_view key,
                         std::int64_t fallback) {
  return jsonAsInteger(memberOrNull(object, key), fallback);
}
double jsonDouble(const Json &object, std::string_view key, double fallback) {
  return jsonAsDouble(memberOrNull(object, key), fallback);
}
bool jsonBool(const Json &object, std::string_view key, bool fallback) {
  return jsonAsBool(memberOrNull(object, key), fallback);
}
Json jsonObject(const Json &object, std::string_view key) {
  return jsonAsObject(memberOrNull(object, key));
}
Json jsonArray(const Json &object, std::string_view key) {
  return jsonAsArray(memberOrNull(object, key));
}
Json jsonValue(const Json &object, std::string_view key) {
  return memberOrNull(object, key);
}

std::vector<std::string> jsonStringList(const Json &value) {
  std::vector<std::string> result;
  if (!value.is_array())
    return result;
  for (const auto &item : value)
    if (item.is_string())
      result.push_back(item.get<std::string>());
  return result;
}

Json jsonTake(Json &object, std::string_view key) {
  if (!object.is_object())
    return nullptr;
  const auto found = object.find(key);
  if (found == object.end())
    return nullptr;
  auto value = std::move(*found);
  object.erase(found);
  return value;
}

Json parseJson(std::string_view text, std::string *error) {
  try {
    return Json::parse(text.begin(), text.end());
  } catch (const Json::exception &exception) {
    if (error)
      *error = exception.what();
    return Json(Json::value_t::discarded);
  }
}

Json parseJsonObject(std::string_view text, std::string *error) {
  auto value = parseJson(text, error);
  if (value.is_discarded())
    return value;
  if (!value.is_object()) {
    if (error)
      *error = "JSON document is not an object";
    return Json(Json::value_t::discarded);
  }
  return value;
}

std::string jsonCompact(const Json &value) {
  return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string jsonIndented(const Json &value) {
  return value.dump(4, ' ', false, Json::error_handler_t::replace) + "\n";
}

Json readJsonFile(const std::filesystem::path &path, std::string *error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (error)
      *error = "Could not open " + path.string();
    return Json(Json::value_t::discarded);
  }
  const std::string text{std::istreambuf_iterator<char>(input),
                         std::istreambuf_iterator<char>()};
  return parseJson(text, error);
}

} // namespace LunaDash
