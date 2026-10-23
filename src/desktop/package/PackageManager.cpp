#include "desktop/package/PackageManager.hpp"

namespace LunaDash {
namespace {

bool firstPackageCharacter(char value) {
  return (value >= 'a' && value <= 'z') ||
         (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') || value == '@' || value == '_' ||
         value == '+';
}

bool packageCharacter(char value) {
  return firstPackageCharacter(value) || value == '.' || value == ':' ||
         value == '-';
}

} // namespace

bool validPackageName(std::string_view package) {
  if (package.empty() || !firstPackageCharacter(package.front()))
    return false;
  for (std::size_t index = 1; index < package.size(); ++index)
    if (!packageCharacter(package[index]))
      return false;
  return true;
}

std::vector<std::string>
packageTransactionArguments(std::string_view operation,
                            std::string_view package) {
  if (operation == "upgrade")
    return {"-Syu"};
  if (!validPackageName(package))
    return {};
  if (operation == "install")
    return {"-Syu", "--", std::string(package)};
  if (operation == "remove")
    return {"-R", "--", std::string(package)};
  return {};
}

} // namespace LunaDash
