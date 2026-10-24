#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace LunaDash {

std::optional<std::string> readFile(const std::filesystem::path &path,
                                    std::size_t maximumBytes = 64u << 20);

// Writes through a sibling temporary file and renames it into place, so
// readers observe either the previous or the complete new contents.
bool writeFileAtomically(const std::filesystem::path &path,
                         std::string_view contents,
                         std::string *error = nullptr,
                         std::filesystem::perms permissions =
                             std::filesystem::perms::owner_read |
                             std::filesystem::perms::owner_write |
                             std::filesystem::perms::group_read |
                             std::filesystem::perms::others_read);

bool createDirectories(const std::filesystem::path &path,
                       std::string *error = nullptr);

// Value of an environment variable, or an empty string when unset.
std::string environmentValue(const char *name);
// Integer value of an environment variable, or fallback when unset/invalid.
int environmentInt(const char *name, int fallback = 0);

} // namespace LunaDash
