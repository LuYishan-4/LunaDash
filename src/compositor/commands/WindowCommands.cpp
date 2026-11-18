#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/commands/CommandArguments.hpp"
#include "compositor/commands/CompositorCommands.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/window/WindowSwitcher.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "core/text/Text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace LunaDash {
namespace {

Json areaJson(const Rect &area) {
  return Json::object({{"x", area.x()},
                       {"y", area.y()},
                       {"width", area.width()},
                       {"height", area.height()}});
}

} // namespace

void CompositorCommands::registerWindowCommands(CommandRegistry &registry) {
  using C = WaylandCompositor;
  using R = CommandRequest;

  registry.add("workspace", [](C &c, const R &r) -> CommandResult {
    const auto number = parseInteger(trimmed(r.value));
    if (!number || *number < 0 || *number >= c.workspaceCount())
      return invalidCommand();
    c.selectWorkspace(*number);
    return {};
  });
  registry.add("launcher-visible", [](C &c, const R &r) -> CommandResult {
    if (r.value != "true" && r.value != "false")
      return commandError("launcher-visible expects true or false.");
    c.setLauncherVisible(r.value == "true", false);
    return {};
  });
  registry.add("scratchpad", [](C &c, const R &) -> CommandResult {
    std::string error;
    if (!c.toggleScratchpad(&error))
      return commandError(error);
    return {};
  });
  registry.add("window-layout-action", [](C &c, const R &r) -> CommandResult {
    const auto object = parseJsonObject(r.value);
    if (!object.is_object() || !c.windowTemplate_)
      return commandError("Expected a window layout action object.");
    const auto actionId = jsonString(object, "action");
    if (!jsonValue(object, "payload").is_object() || object.size() != 2)
      return commandError("Expected only action and payload fields.");
    auto payload = jsonObject(object, "payload");
    const auto action = jsonObject(c.windowTemplate_->layoutActions, actionId);
    if (jsonBool(action, "requiresArea")) {
      if (jsonContains(payload, "area"))
        return commandError(
            "Work area is supplied by the compositor, not the caller.");
      payload["area"] = areaJson(c.workArea());
    }
    if (actionId.empty() || !c.performLayoutAction(actionId, payload))
      return commandError("Window layout action is unsupported or invalid.");
    c.arrange();
    c.synchronizeWindowFocus();
    return {};
  });
  registry.add("window-layout-settings", [](C &c, const R &r) -> CommandResult {
    const auto document = parseJsonObject(r.value);
    if (!document.is_object())
      return commandError("Expected a JSON object of window layout settings.");
    std::string error;
    if (!c.updateWindowLayoutSettings(document, &error))
      return commandError(error);
    c.arrange();
    return {};
  });
  registry.add("group-window", [](C &c, const R &r) -> CommandResult {
    int window = 0;
    int target = 0;
    if (!groupWindowIds(r.value, &window, &target))
      return commandError("Expected bounded JSON window and target IDs.");
    if (!c.performLayoutAction("group-with", {{"window", window}, {"target", target}}))
      return commandError("Windows cannot be grouped.");
    c.arrange();
    return {};
  });
  registry.add("expel-window", [](C &c, const R &r) -> CommandResult {
    const auto window = textWindowId(r.value);
    if (!window || !c.performLayoutAction("expel", {{"window", *window}}))
      return commandError("Window layout does not support expelling this window.");
    c.arrange();
    c.synchronizeWindowFocus();
    return {};
  });

  // The workspace overview: "toggle", "open" or "close".
  registry.add("overview", [](C &c, const R &r) -> CommandResult {
    if (r.value != "toggle" && r.value != "open" && r.value != "close")
      return commandError("Overview expects toggle, open or close.");
    if (r.value == "toggle" || (r.value == "open") != c.overview_.open)
      c.toggleOverview();
    return {};
  });

  // Window switcher (Alt+Tab and the shell's overview).
  registry.add("switch-window", [](C &c, const R &r) -> CommandResult {
    const auto id = textWindowId(r.value);
    if (!id || !c.windowSwitcher_->select(*id))
      return commandError("Unknown selection");
    return {};
  });
  registry.add("switch-begin", [](C &c, const R &r) -> CommandResult {
    if (r.value != "windows" && r.value != "workspaces")
      return invalidCommand();
    // Opened from the shell: start on the current entry.
    c.beginWindowSwitch(0, r.value == "workspaces");
    return {};
  });
  registry.add("switch-step", [](C &c, const R &r) -> CommandResult {
    c.windowSwitcher_->step(r.value == "previous" ? -1 : 1);
    return {};
  });
  registry.add("switch-accept", [](C &c, const R &) -> CommandResult {
    c.finishWindowSwitch(true);
    return {};
  });
  registry.add("switch-cancel", [](C &c, const R &) -> CommandResult {
    c.finishWindowSwitch(false);
    return {};
  });

  // Task list actions on one window.
  registry.add("activate-window", [](C &c, const R &r) -> CommandResult {
    const auto id = textWindowId(r.value);
    if (!id)
      return commandError("Expected a window ID");
    c.activateTask(*id);
    return {};
  });
  const auto windowById = [](C &c, const R &r, ClientWindow **client) -> CommandResult {
    const auto id = textWindowId(r.value);
    if (!id)
      return commandError("Window command requires a positive integer ID.");
    *client = c.clientById(*id);
    if (!*client)
      return commandError("Unknown window");
    return {};
  };
  registry.add("close", [windowById](C &c, const R &r) -> CommandResult {
    ClientWindow *client = nullptr;
    if (auto error = windowById(c, r, &client))
      return error;
    closeClient(client);
    return {};
  });
  registry.add("minimize", [windowById](C &c, const R &r) -> CommandResult {
    ClientWindow *client = nullptr;
    if (auto error = windowById(c, r, &client))
      return error;
    c.minimizeClient(client);
    c.arrange();
    c.synchronizeWindowFocus();
    return {};
  });
  registry.add("focus", [windowById](C &c, const R &r) -> CommandResult {
    ClientWindow *client = nullptr;
    if (auto error = windowById(c, r, &client))
      return error;
    if (client->workspace != c.workspace_)
      c.workspaceSlide_ = client->workspace > c.workspace_ ? 1 : -1;
    c.workspace_ = client->workspace;
    client->minimized = false;
    c.windowLayout_->setMinimized(client->id, false);
    c.windowLayout_->focus(client->id);
    c.arrange();
    c.focus(client);
    return {};
  });
}

} // namespace LunaDash
