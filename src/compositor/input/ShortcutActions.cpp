#include "compositor/input/ShortcutActions.hpp"

#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "core/json/JsonAccess.hpp"
#include "core/log/Log.hpp"
#include "desktop/app/DefaultApplications.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>

namespace LunaDash {
namespace {

Json areaJson(const Rect &area) {
  return Json::object({{"x", area.x()},
                       {"y", area.y()},
                       {"width", area.width()},
                       {"height", area.height()}});
}


int actionNumber(std::string_view action, std::string_view prefix) {
  int number = 0;
  const auto suffix = action.substr(prefix.size());
  const auto parsed =
      std::from_chars(suffix.data(), suffix.data() + suffix.size(), number);
  return parsed.ec == std::errc{} && parsed.ptr == suffix.data() + suffix.size()
             ? number
             : 0;
}

} // namespace

void ShortcutActions::focusDirection(WaylandCompositor &c, int dx, int dy) {
  c.performLayoutAction("focus-direction",
                        {{"workspace", c.workspace_}, {"dx", dx}, {"dy", dy}});
}

void ShortcutActions::reorder(WaylandCompositor &c, int direction) {
  c.performLayoutAction("reorder",
                        {{"window", c.focused_->id}, {"direction", direction}});
}

void ShortcutActions::groupDirection(WaylandCompositor &c, int dx) {
  c.performLayoutAction("group-direction", {{"window", c.focused_->id},
                                            {"workspace", c.workspace_},
                                            {"dx", dx},
                                            {"dy", 0}});
}

void ShortcutActions::resizeColumn(WaylandCompositor &c, int width) {
  c.performLayoutAction("resize-width",
                        {{"window", c.focused_->id}, {"width", width}});
}

void ShortcutActions::toggleLauncher(WaylandCompositor &c) {
  // Without the LunaDash shell the launcher is whatever session.json names.
  if (c.session_.shell != SessionConfig::Shell::LunaDash) {
    std::string error;
    if (!c.session_.launcherCommand.empty() &&
        !c.launchExternalCommand(c.session_.launcherCommand, &error))
      logWarning("The launcher could not start: %s", error.c_str());
    return;
  }

  c.setLauncherVisible(!c.launcherVisible_);
}

void ShortcutActions::registerBuiltins(ShortcutRegistry &registry) {
  using C = WaylandCompositor;
  auto &actions = registry.actions;
  const auto add = [&actions](const char *name, unsigned flags, auto run) {
    actions.add(name, ShortcutAction{flags, run});
  };
  // A shortcut that performs a control-channel request.
  const auto request = [](const char *method, const char *value = "") {
    return [method, value](C &c, std::string_view) {
      c.control({{"method", method}, {"value", value}});
    };
  };
  constexpr unsigned column = ShortcutNeedsFocus | ShortcutLeavesMaximized;

  add("focusLeft", ShortcutNone, [](C &c, auto) { focusDirection(c, -1, 0); });
  add("focusRight", ShortcutNone, [](C &c, auto) { focusDirection(c, 1, 0); });
  add("focusUp", ShortcutNone, [](C &c, auto) { focusDirection(c, 0, -1); });
  add("focusDown", ShortcutNone, [](C &c, auto) { focusDirection(c, 0, 1); });
  add("reorderLeft", column, [](C &c, auto) { reorder(c, -1); });
  add("reorderRight", column, [](C &c, auto) { reorder(c, 1); });
  add("groupLeft", column, [](C &c, auto) { groupDirection(c, -1); });
  add("groupRight", column, [](C &c, auto) { groupDirection(c, 1); });
  add("expelWindow", column, [](C &c, auto) {
    c.performLayoutAction("expel", {{"window", c.focused_->id}});
  });
  add("centerColumn", column, [](C &c, auto) {
    c.performLayoutAction("center", {{"window", c.focused_->id},
                                     {"area", areaJson(c.workArea())}});
  });
  add("widenColumn", column, [](C &c, auto) {
    resizeColumn(c, std::min(c.workArea().width(),
                             std::max(WindowTuning::columnMinimumWidth,
                                      c.focused_->geometry.width() +
                                          WindowTuning::columnResizeStep)));
  });
  add("narrowColumn", column, [](C &c, auto) {
    resizeColumn(c, std::max(WindowTuning::columnMinimumWidth,
                             c.focused_->geometry.width() -
                                 WindowTuning::columnResizeStep));
  });
  add("maximizeWindow", ShortcutNeedsFocus,
      [](C &c, auto) { c.setMaximized(c.focused_, !c.focused_->maximized); });
  add("toggleFullscreen", ShortcutNeedsFocus,
      [](C &c, auto) { c.setFullscreen(c.focused_, !c.focused_->fullscreen); });
  add("closeWindow", ShortcutNeedsFocus, [](C &c, auto) { closeClient(c.focused_); });
  add("minimizeWindow", ShortcutNeedsFocus, [](C &c, auto) {
    c.focused_->minimized = true;
    c.windowLayout_->setMinimized(c.focused_->id, true);
  });
  add("launchTerminal", ShortcutNone, request("launch-default", DefaultApplicationRole::terminal.data()));
  add("launchFiles", ShortcutNone, request("launch-default", DefaultApplicationRole::files.data()));
  add("toggleScratchpad", ShortcutNoRelayout, [](C &c, auto) {
    std::string error;
    c.toggleScratchpad(&error);
    if (!error.empty())
      c.scratchpad_.error = error;
  });
  add("toggleEyeCare", ShortcutNone, request("eye-care", "toggle"));
  add("chooseWallpaper", ShortcutNone, request("choose-wallpaper"));
  add("randomWallpaper", ShortcutNone, request("wallpaper-random"));
  // Shell surfaces the shell opens itself: the compositor relays the action name.
  for (const char *name :
       {"launchOrbit", "openControlCenter", "openClipboard", "openPowerMenu"})
    add(name, ShortcutNone,
        [](C &c, std::string_view action) { c.requestShellAction(action); });
  add("launchLauncher", ShortcutNoRelayout, [](C &c, auto) { toggleLauncher(c); });
  // The workspace overview; Super tap and Super+Tab by default.
  add("toggleOverview", ShortcutNoRelayout, [](C &c, auto) { c.toggleOverview(); });
  for (const char *name :
       {"overviewClose", "overviewAccept", "overviewAcceptKeypad",
        "overviewFocusLeft", "overviewFocusLeftVim", "overviewFocusRight",
        "overviewFocusRightVim", "overviewWorkspacePrevious",
        "overviewWorkspacePreviousVim", "overviewWorkspaceNext",
        "overviewWorkspaceNextVim", "overviewReorderLeft",
        "overviewReorderLeftVim", "overviewReorderRight",
        "overviewReorderRightVim", "overviewMoveToPreviousWorkspace",
        "overviewMoveToPreviousWorkspaceVim", "overviewMoveToNextWorkspace",
        "overviewMoveToNextWorkspaceVim"})
    add(name, ShortcutNoRelayout,
        [](C &c, std::string_view action) { c.handleOverviewShortcut(action); });
  // A repeated shortcut must leave keyboard focus on the active selector so
  // Escape can still cancel it.
  add("screenshot", ShortcutNoRelayout, [](C &c, auto) { c.captureScreen(); });

  // Beyond the last workspace: the empty one after it.
  registry.numbered.add("workspace", NumberedShortcutAction{[](C &c, int number) {
    if (number > 0)
      c.selectWorkspace(std::min(number, c.workspaceCount()) - 1);
  }});
  registry.numbered.add("moveToWorkspace", NumberedShortcutAction{[](C &c, int number) {
    if (number > 0 && c.focused_)
      c.moveClientToWorkspace(c.focused_, std::min(number, c.workspaceCount()) - 1);
  }});
}

void WaylandCompositor::handleShortcut(std::string_view action) {
  for (const auto &prefix : shortcuts_.numbered.names())
    if (action.starts_with(prefix)) {
      shortcuts_.numbered.find(prefix)->run(*this, actionNumber(action, prefix));
      return;
    }
  if (const auto *shortcut = shortcuts_.actions.find(action)) {
    if ((shortcut->flags & ShortcutLeavesMaximized) && focused_ &&
        focused_->maximized) {
      setMaximized(focused_, false);
      arrange();
    }
    if (!(shortcut->flags & ShortcutNeedsFocus) || focused_) {
      shortcut->run(*this, action);
      if (shortcut->flags & ShortcutNoRelayout)
        return;
    }
  }
  arrange();
  synchronizeWindowFocus();
}

void WaylandCompositor::applyKeyboardConfiguration() {
  if (d)
    d->applyKeyboardConfig();
}

void WaylandCompositor::resendKeyboardModifiers() {
  if (!d || !d->seat)
    return;
  if (auto *keyboard = wlr_seat_get_keyboard(d->seat)) {
    wlr_seat_keyboard_notify_modifiers(d->seat, &keyboard->modifiers);
    ++keyboardDiagnostics_.modifierResends;
  }
}

} // namespace LunaDash
