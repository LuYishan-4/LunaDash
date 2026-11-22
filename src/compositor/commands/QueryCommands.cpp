#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "desktop/audio/AudioSettings.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/input/InputSettings.hpp"
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/KeyBindings.hpp"

namespace LunaDash {

// One request per topic, as sway's get_* requests; `subscribe` follows them.
void CompositorCommands::registerQueryCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  registry.add("get-version", [](C &c, const R &) -> CommandResult {
    return c.describeVersion();
  });
  registry.add("get-workspaces", [](C &c, const R &) -> CommandResult {
    return c.describeWorkspaces();
  });
  registry.add("get-clients", [](C &c, const R &) -> CommandResult {
    Json xwaylandUtilities = Json::array();
    return Json{{"clients", c.describeClients(xwaylandUtilities)}};
  });
  registry.add("get-layout", [](C &c, const R &) -> CommandResult {
    return c.describeLayout();
  });
  registry.add("get-outputs", [](C &c, const R &) -> CommandResult {
    return c.d ? c.d->displaySnapshot() : Json::object();
  });
  registry.add("get-inputs", [](C &c, const R &) -> CommandResult {
    return c.describeInput(desktopPreferences());
  });
  registry.add("get-keybindings", [](C &c, const R &) -> CommandResult {
    return Json{{"keybindings", c.keyBindings_->snapshot()}};
  });
  registry.add("get-appearance", [](C &c, const R &) -> CommandResult {
    return c.describeAppearance();
  });
  registry.add("get-wallpaper", [](C &c, const R &) -> CommandResult {
    return c.describeWallpaper();
  });
  registry.add("get-devices", [](C &c, const R &) -> CommandResult {
    // Read on request rather than polled: this reply has the values as last
    // read, and the next one the values read now.
    c.audioSettings_->refresh();
    c.brightnessSettings_->refresh();
    c.powerSettings_->refresh();
    return c.describeDevices();
  });
  registry.add("get-session", [](C &c, const R &) -> CommandResult {
    return c.describeSession();
  });
  registry.add("get-renderer", [](C &c, const R &) -> CommandResult {
    return c.describeRenderer();
  });
  registry.add("get-extensions", [](C &c, const R &) -> CommandResult {
    return c.describeExtensions();
  });
  registry.add("get-settings", [](C &c, const R &) -> CommandResult {
    return c.describeSettings();
  });
}

} // namespace LunaDash
