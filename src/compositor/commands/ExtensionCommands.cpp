#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "config/plugins/ExtensionConfiguration.hpp"

#include <string>

namespace LunaDash {

void CompositorCommands::registerExtensionCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  registry.add("extension-save", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!saveExtensionConfiguration(r.value, &error))
      return commandError(error);
    c.pluginManager_->refresh();
    c.arrange();
    return {};
  });
  registry.add("extension-install", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!c.pluginManager_->installFromStore(r.value, &error))
      return commandError(error);
    return Json{{"pending", true}, {"pluginId", r.value}};
  });
  registry.add("extension-remove", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!c.pluginManager_->removeInstalledPlugin(r.value, &error))
      return commandError(error);
    return Json{{"removed", true}, {"pluginId", r.value}};
  });
  // A plugin's UI reports a failure it hit.
  registry.add("extension-error", [](C &c, const R &r) -> CommandResult {
    const auto object = parseJsonObject(r.value);
    c.pluginManager_->reportError(jsonString(object, "id"),
                                  jsonString(object, "error"));
    return {};
  });
}

} // namespace LunaDash
