#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

using ShellModuleJson = nlohmann::json;

const ShellModuleJson &shellModuleDescriptors();
ShellModuleJson shellModuleDescriptor(std::string_view id);
std::vector<std::string> shellModuleIds();
ShellModuleJson defaultModuleDocument();
bool validateModuleDocument(std::string_view text,
                            ShellModuleJson *normalized,
                            std::string *error);

} // namespace LunaDash
