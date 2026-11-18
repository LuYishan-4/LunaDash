#include "compositor/commands/CommandArguments.hpp"
#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "core/text/Text.hpp"
#include "desktop/audio/AudioSettings.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"
#include "desktop/power/PowerSettings.hpp"

#include <string>

namespace LunaDash {

void CompositorCommands::registerDeviceCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  registry.add("audio", [](C &c, const R &r) -> CommandResult {
    const auto request = parseJsonObject(r.value);
    std::string error;
    if (!request.is_object() || !c.audioSettings_->apply(request, &error))
      return commandError(error.empty() ? "Invalid audio setting." : error);
    return {};
  });
  registry.add("power-profile", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!c.powerSettings_->apply(r.value, &error))
      return commandError(error);
    return {};
  });
  registry.add("brightness", [](C &c, const R &r) -> CommandResult {
    const auto percent = parseInteger(trimmed(r.value));
    std::string error;
    if (!percent || !c.brightnessSettings_->setPercent(*percent, &error))
      return commandError(error.empty() ? "Brightness must be an integer." : error);
    return {};
  });
  registry.add("ddc-brightness", [](C &c, const R &r) -> CommandResult {
    const auto request = parseJsonObject(r.value);
    const auto percent = jsonValue(request, "percent");
    std::string error;
    if (!request.is_object() || !percent.is_number() ||
        jsonAsDouble(percent) != jsonAsInt(percent, -1) ||
        !c.ddcBrightnessSettings_->setPercent(jsonString(request, "id"),
                                              jsonAsInt(percent, -1), &error))
      return commandError(error.empty() ? "Invalid DDC/CI brightness request."
                                        : error);
    return {};
  });
  registry.add("ddc-refresh", [](C &c, const R &) -> CommandResult {
    c.ddcBrightnessSettings_->refresh();
    return {};
  });
  registry.add("display-configure", [](C &c, const R &r) -> CommandResult {
    const auto changes = parseJsonObject(r.value);
    std::string error;
    if (!changes.is_object() || !c.d->configureDisplay(changes, &error))
      return commandError(error.empty() ? "Invalid display configuration." : error);
    return {};
  });
  registry.add("display-confirm", [](C &c, const R &) -> CommandResult {
    c.d->confirmDisplay();
    return {};
  });
  registry.add("display-revert", [](C &c, const R &) -> CommandResult {
    c.d->revertDisplay();
    return {};
  });
  // The nested (virtual output) desktop size, e.g. 1440x900.
  registry.add("desktop-size", [](C &c, const R &r) -> CommandResult {
    std::string error;
    if (!c.d || !c.d->resizePrimaryOutput(r.value, &error))
      return commandError(error);
    return {};
  });
}

} // namespace LunaDash
