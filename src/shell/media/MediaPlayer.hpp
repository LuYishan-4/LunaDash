#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace LunaDash {
nlohmann::json mediaStatus(const std::string &preferredService = {},
                           const std::string &preferredBus = {});
nlohmann::json mediaAction(const std::string &action, const std::string &service,
                          const std::string &bus,
                          const std::string &value = {},
                          const std::string &trackId = {});
} // namespace LunaDash
