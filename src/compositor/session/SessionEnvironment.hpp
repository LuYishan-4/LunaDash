#pragma once

#include "core/process/ProcessEnvironment.hpp"

#include <nlohmann/json_fwd.hpp>

namespace LunaDash {

using ClientEnvironment = ProcessEnvironment;

ClientEnvironment createClientEnvironment(const std::string &socketName,
                                          const std::string &controlPath,
                                          const std::string &binaryDirectory,
                                          const nlohmann::json &preferences);
bool publishClientEnvironment(const ClientEnvironment &environment);

} // namespace LunaDash
