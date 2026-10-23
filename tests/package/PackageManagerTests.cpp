#include "desktop/package/PackageManager.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main() {
  using LunaDash::packageTransactionArguments;
  using LunaDash::validPackageName;

  for (const auto name : {"firefox", "lib32-zlib", "fcitx5-gtk", "@group",
                          "+debug"}) {
    if (!validPackageName(name)) {
      std::cerr << "A valid package name was rejected: " << name << '\n';
      return 1;
    }
  }
  for (const auto name : {"", "-Syu", "../pacman", "name;touch-file",
                          "two packages", "name\\n"}) {
    if (validPackageName(name)) {
      std::cerr << "An invalid package name was accepted: " << name << '\n';
      return 1;
    }
  }

  if (packageTransactionArguments("install", "firefox") !=
          std::vector<std::string>{"-Syu", "--", "firefox"} ||
      packageTransactionArguments("remove", "firefox") !=
          std::vector<std::string>{"-R", "--", "firefox"} ||
      packageTransactionArguments("upgrade") !=
          std::vector<std::string>{"-Syu"} ||
      !packageTransactionArguments("install", "--help").empty() ||
      !packageTransactionArguments("unknown", "firefox").empty()) {
    std::cerr << "Package transaction argument contract failed.\n";
    return 1;
  }
  std::cout << "Native package name and transaction validation passed.\n";
  return 0;
}
