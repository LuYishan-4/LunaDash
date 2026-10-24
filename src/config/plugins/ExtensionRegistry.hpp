#pragma once
#include "core/json/JsonAccess.hpp"

#include <string>

namespace LunaDash {
Json extensionTargets();
Json extensionTarget(const std::string &id);
bool validateExtensionSettings(const Json &schema, const Json &settings,
                               std::string *error);
Json extensionDefaults(const Json &schema);
} // namespace LunaDash
