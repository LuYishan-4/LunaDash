#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

// Roles a default application can fill.
namespace DefaultApplicationRole {
inline constexpr std::string_view terminal = "terminal";
inline constexpr std::string_view files = "files";
inline constexpr std::string_view browser = "browser";
} // namespace DefaultApplicationRole

bool isDefaultApplicationRole(std::string_view role);

nlohmann::json defaultApplications();
bool setDefaultApplications(const nlohmann::json &changes, std::string *error);
std::vector<std::string> defaultApplicationCommand(std::string_view role,
                                                   std::string *error);
std::vector<std::string> konsoleCommand();

} // namespace LunaDash
