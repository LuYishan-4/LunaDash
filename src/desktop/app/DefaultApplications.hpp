#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

nlohmann::json defaultApplications();
bool setDefaultApplications(const nlohmann::json &changes, std::string *error);
std::vector<std::string> defaultApplicationCommand(std::string_view role,
                                                   std::string *error);
std::vector<std::string> konsoleCommand();

} // namespace LunaDash
