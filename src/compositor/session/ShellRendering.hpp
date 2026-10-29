#pragma once
#include <map>
#include <string>
namespace LunaDash {
using ShellRenderingEnvironment = std::map<std::string, std::string>;
// The Slint shell's renderer: translates LUDASH_SHELL_RENDERER into SLINTLU_RENDERER.
// "auto" keeps an explicit SLINTLU_RENDERER, otherwise SlintLu chooses and falls back
// itself. False (environment unchanged) for an unknown renderer.
bool configureSlintLuRendering(ShellRenderingEnvironment &environment);
} // namespace LunaDash
