#pragma once
#include "core/json/JsonAccess.hpp"

#include <string>

namespace LunaDash {
std::string extensionConfigurationPath();
Json readExtensionConfiguration(std::string *error = nullptr);
bool saveExtensionConfiguration(const std::string &json, std::string *error);
Json configuredBuiltinSettings(const std::string &target);
Json configuredBuiltinSettings(const std::string &target, const Json &document);
} // namespace LunaDash
