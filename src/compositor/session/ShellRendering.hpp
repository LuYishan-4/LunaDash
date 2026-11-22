#pragma once
#include <map>
#include <string>
namespace LunaDash {
using ShellRenderingEnvironment = std::map<std::string, std::string>;

bool configureSlintLuRendering(ShellRenderingEnvironment &environment);
} // namespace LunaDash
