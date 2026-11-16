#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "core/process/ExecutableLookup.hpp"

namespace LunaDash {

void CompositorCommands::registerCaptureCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  // Saves the whole output to an absolute path the caller chose.
  registry.add("capture", [](C &c, const R &r) -> CommandResult {
    if (!r.value.starts_with('/'))
      return commandError("Capture requires an absolute path.");
    if (fileExists(r.value))
      return commandError("Refusing to overwrite " + r.value);
    if (!c.saveScreenshot(r.value))
      return commandError(c.capture_.error.empty()
                              ? "Could not start capture for " + r.value
                              : c.capture_.error);
    return Json{{"path", r.value},
                {"pending", true},
                {"phase", c.screenCapture_->phase()}};
  });
  // Interactive region selection into the Screenshots directory.
  registry.add("screenshot", [](C &c, const R &) -> CommandResult {
    c.captureScreen();
    if (!c.capture_.error.empty())
      return commandError(c.capture_.error);
    return Json{{"pending", c.screenCapture_->busy()},
                {"phase", c.screenCapture_->phase()}};
  });
}

} // namespace LunaDash
