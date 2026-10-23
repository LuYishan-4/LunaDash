#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

bool validPackageName(std::string_view package);
std::vector<std::string>
packageTransactionArguments(std::string_view operation,
                            std::string_view package = {});

} // namespace LunaDash
