#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

using Json = nlohmann::json;

// Lenient typed reads for JSON documents received from files, IPC and
// plugins. A missing member or a value of another type yields the fallback,
// so callers never throw on malformed input. Integer reads accept integral
// floating-point values (for example 12.0) and reject fractional ones.
const Json *jsonMember(const Json &object, std::string_view key);
bool jsonContains(const Json &object, std::string_view key);

std::string jsonAsString(const Json &value, std::string fallback = {});
int jsonAsInt(const Json &value, int fallback = 0);
std::int64_t jsonAsInteger(const Json &value, std::int64_t fallback = 0);
double jsonAsDouble(const Json &value, double fallback = 0.0);
bool jsonAsBool(const Json &value, bool fallback = false);
Json jsonAsObject(const Json &value);
Json jsonAsArray(const Json &value);
bool jsonIsIntegral(const Json &value);

std::string jsonString(const Json &object, std::string_view key,
                       std::string fallback = {});
int jsonInt(const Json &object, std::string_view key, int fallback = 0);
std::int64_t jsonInteger(const Json &object, std::string_view key,
                         std::int64_t fallback = 0);
double jsonDouble(const Json &object, std::string_view key,
                  double fallback = 0.0);
bool jsonBool(const Json &object, std::string_view key, bool fallback = false);
Json jsonObject(const Json &object, std::string_view key);
Json jsonArray(const Json &object, std::string_view key);
Json jsonValue(const Json &object, std::string_view key);
std::vector<std::string> jsonStringList(const Json &value);

// Removes and returns a member; returns null when the member is absent.
Json jsonTake(Json &object, std::string_view key);

// Parsing that never throws. Returns a discarded value on error and
// stores a human-readable reason in error when it is provided.
Json parseJson(std::string_view text, std::string *error = nullptr);
Json parseJsonObject(std::string_view text, std::string *error = nullptr);
std::string jsonCompact(const Json &value);
std::string jsonIndented(const Json &value);

Json readJsonFile(const std::filesystem::path &path,
                  std::string *error = nullptr);

} // namespace LunaDash
