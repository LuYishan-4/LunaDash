#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"

namespace LunaDash {

void CompositorCommands::registerBuiltins(CommandRegistry &registry) {
  registerSessionCommands(registry);
  registerCaptureCommands(registry);
  registerWindowCommands(registry);
  registerLaunchCommands(registry);
  registerSettingsCommands(registry);
  registerWallpaperCommands(registry);
  registerDeviceCommands(registry);
  registerExtensionCommands(registry);
  registerQueryCommands(registry);
}

Json WaylandCompositor::control(const Json &request) {
  const std::string method = jsonString(request, "method");
  const std::string value = jsonString(request, "value");
  const auto *handler = commands_.find(method);
  if (!handler)
    return *invalidCommand();
  auto result = (*handler)(*this, CommandRequest{method, value, request});
  return result ? std::move(*result) : Json{{"success", true}};
}

} // namespace LunaDash
