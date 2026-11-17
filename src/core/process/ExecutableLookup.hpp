#pragma once

#include <string>
#include <string_view>

namespace LunaDash {

// Path of an executable found on PATH, or an empty string.
std::string findExecutable(std::string_view program);
std::string findExecutable(std::string_view program,
                           std::string_view searchPath);
// Directory of the running executable (/proc/self/exe), or an empty string.
std::string executableDirectory();
bool fileExists(const std::string &path);
bool fileIsExecutable(const std::string &path);

} // namespace LunaDash
