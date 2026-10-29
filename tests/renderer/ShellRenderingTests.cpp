#include "compositor/session/ShellRendering.hpp"

#include <iostream>

namespace LunaDash::Tests {
namespace {
bool check(bool condition, const char *message) {
  if (!condition)
    std::cerr << message << '\n';
  return condition;
}

bool slintLuSelection() {
  ShellRenderingEnvironment environment;
  environment["LUDASH_SHELL_RENDERER"] = "vulkan";
  if (!check(configureSlintLuRendering(environment) &&
                 environment.at("SLINTLU_RENDERER") == "vulkan",
             "the SlintLu shell did not receive an explicit renderer"))
    return false;
  environment.clear();
  environment["SLINTLU_RENDERER"] = "software";
  if (!check(configureSlintLuRendering(environment) &&
                 environment.at("SLINTLU_RENDERER") == "software",
             "automatic selection overwrote the caller's SlintLu renderer"))
    return false;
  environment.clear();
  if (!check(configureSlintLuRendering(environment) &&
                 !environment.contains("SLINTLU_RENDERER"),
             "automatic selection must leave the choice to SlintLu"))
    return false;
  environment["LUDASH_SHELL_RENDERER"] = "typo";
  const auto original = environment;
  return check(!configureSlintLuRendering(environment) && environment == original,
               "an invalid SlintLu renderer selection was accepted");
}
} // namespace

int run() {
  return slintLuSelection() ? 0 : 1;
}
} // namespace LunaDash::Tests

int main() { return LunaDash::Tests::run(); }
