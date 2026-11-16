#pragma once

#include <string>

namespace LunaDash {

// Absolute path of an executable found on PATH, or an empty string.
std::string findExecutable(const std::string &name);
// Directory of the running executable (/proc/self/exe), or an empty string.
std::string executableDirectory();
bool fileExists(const std::string &path);
bool fileIsExecutable(const std::string &path);

} // namespace LunaDash
