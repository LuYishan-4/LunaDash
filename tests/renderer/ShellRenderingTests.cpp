#include "shell/runtime/ShellRenderer.hpp"

#include <iostream>

namespace LunaDash::Tests {
namespace {
bool check(bool condition, const char *message) {
  if (!condition)
    std::cerr << message << '\n';
  return condition;
}

bool explicitVulkan() {
  ShellRenderingEnvironment environment;
  environment["LUDASH_SHELL_RENDERER"] = "vulkan";
  environment["QT_QUICK_BACKEND"] = "software";
  return check(configureShellRendering(environment, true),
               "explicit Vulkan selection was rejected") &&
         check(environment.at("QSG_RHI_BACKEND") == "vulkan",
               "explicit Vulkan renderer was not preserved") &&
         check(!environment.contains("QT_QUICK_BACKEND"),
               "software Qt Quick override was not cleared");
}

bool automaticSelection() {
  ShellRenderingEnvironment environment;
  if (!configureShellRendering(environment, true) ||
      environment.at("QT_QUICK_BACKEND") != "software")
    return check(false, "NVIDIA automatic fallback was not applied");
  environment.clear();
  return configureShellRendering(environment, false) &&
         check(environment.at("QSG_RHI_BACKEND") == "opengl",
               "OpenGL was not selected for the default shell");
}

bool preserveToolkitChoice() {
  ShellRenderingEnvironment environment;
  environment["QSG_RHI_BACKEND"] = "vulkan";
  return configureShellRendering(environment, true) &&
         check(environment.at("QSG_RHI_BACKEND") == "vulkan",
               "the caller's Qt Quick renderer was overwritten");
}

bool invalidChoiceDoesNotMutateEnvironment() {
  ShellRenderingEnvironment environment;
  environment["LUDASH_SHELL_RENDERER"] = "typo";
  const auto original = environment;
  return check(!configureShellRendering(environment, false),
               "invalid renderer selection was accepted") &&
         check(environment == original,
               "invalid renderer selection modified the environment");
}
} // namespace

int run() {
  if (!explicitVulkan() || !automaticSelection() || !preserveToolkitChoice() ||
      !invalidChoiceDoesNotMutateEnvironment())
    return 1;
  return 0;
}
} // namespace LunaDash::Tests

int main() { return LunaDash::Tests::run(); }
