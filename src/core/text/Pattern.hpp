#pragma once

#include <string>

namespace LunaDash {


bool patternIsValid(const std::string &pattern);
bool patternMatches(const std::string &pattern, const std::string &subject,
                    bool *valid = nullptr);

} // namespace LunaDash
