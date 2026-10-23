#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace LunaDash {

std::string selectedLanguage();
bool isSupportedLanguage(std::string_view language);
nlohmann::json languageDictionary(std::string_view language);
bool setSelectedLanguage(std::string_view language, std::string *error);

} // namespace LunaDash
