#include "compositor/input/CompositorActions.hpp"

#include "compositor/client/ClientWindow.hpp"
#include "compositor/client/ClientWindowControl.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/WindowTuning.hpp"
#include "core/json/JsonAccess.hpp"
#include "core/log/Log.hpp"
#include "desktop/app/DefaultApplications.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>

namespace LunaDash {
namespace {

using C = WaylandCompositor;
using Arguments = std::vector<std::string>;

Json areaJson(const Rect &area) {
  return Json::object({{"x", area.x()},
                       {"y", area.y()},
                       {"width", area.width()},
                       {"height", area.height()}});
}

std::optional<int> number(std::string_view text) {
  int value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    return std::nullopt;
  return value;
}


struct WidthChange {
  bool relative = false;
  bool percent = false;
  int value = 0;
};
//Meta+equal
std::optional<WidthChange> widthChange(std::string_view text) {
  WidthChange change;
  if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
    change.relative = true;
    if (text.front() == '+')
      text.remove_prefix(1);
  }
  if (!text.empty() && text.back() == '%') {
    change.percent = true;
    text.remove_suffix(1);
  }
  const auto value = number(text);
  if (!value || (!change.relative && *value <= 0))
    return std::nullopt;
  change.value = *value;
  return change;
}

} // namespace

void CompositorActions::focusDirection(C &c, int dx, int dy) {
  c.performLayoutAction("focus-direction",
                        {{"workspace", c.workspace_}, {"dx", dx}, {"dy", dy}});
}

int CompositorActions::existingWorkspace(C &c, int workspace) {
  return std::clamp(workspace, 0, std::max(0, c.workspaceCount() - 1));
}

std::optional<ActionCall> parseActionCall(std::string_view text, std::string *error) {
  ActionCall call;
  std::string word;
  bool inWord = false;
  char quote = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (quote) {
      if (ch == quote)
        quote = 0;
      else if (ch == '\\' && quote == '"' && i + 1 < text.size())
        word += text[++i];
      else
        word += ch;
    } else if (ch == '"' || ch == '\'') {
      quote = ch;
      inWord = true;
    } else if (ch == '\\' && i + 1 < text.size()) {
      word += text[++i];
      inWord = true;
    } else if (ch == ' ' || ch == '\t') {
      if (inWord)
        (call.name.empty() ? call.name : call.arguments.emplace_back()) = std::move(word);
      word.clear();
      inWord = false;
    } else {
      word += ch;
      inWord = true;
    }
  }
  if (quote) {
    if (error)
      *error = "Unclosed quote in: " + std::string(text);
    return std::nullopt;
  }
  if (inWord)
    (call.name.empty() ? call.name : call.arguments.emplace_back()) = std::move(word);
  if (call.name.empty()) {
    if (error)
      *error = "Expected an action.";
    return std::nullopt;
  }
  return call;
}

bool checkActionCall(const ActionRegistry &registry, const ActionCall &call,
                     std::string *error) {
  const auto fail = [error](std::string message) {
    if (error)
      *error = std::move(message);
    return false;
  };
  const auto *action = registry.find(call.name);
  if (!action)
    return fail("Unknown action: " + call.name);
  const auto &arguments = call.arguments;
  switch (action->argument) {
  case ActionArgument::None:
    return arguments.empty() || fail(call.name + " takes no argument.");
  case ActionArgument::Number:
    return (arguments.size() == 1 && number(arguments.front()).value_or(0) > 0) ||
           fail(call.name + " takes a number from 1.");
  case ActionArgument::Word:
    return (arguments.size() == 1 &&
            (!action->accepts || action->accepts(arguments.front()))) ||
           fail(call.name + " does not take " +
                (arguments.empty() ? std::string("nothing") : arguments.front()) + ".");
  case ActionArgument::Command:
    return !arguments.empty() || fail(call.name + " needs a command.");
  }
  return fail("Unknown action: " + call.name);
}

void CompositorActions::registerBuiltins(ActionRegistry &registry) {
  const auto add = [&registry](const char *name, ActionArgument argument,
                               unsigned flags, auto run,
                               std::function<bool(std::string_view)> accepts = {}) {
    registry.add(name, CompositorAction{argument, flags, run, std::move(accepts)});
  };
  const auto none = ActionArgument::None;
  constexpr unsigned column = ActionNeedsFocus | ActionLeavesMaximized;
  // A control-channel request (the same lunadashctl sends).
  const auto request = [](const char *method, const char *value = "") {
    return [method, value](C &c, const Arguments &) {
      c.control({{"method", method}, {"value", value}});
    };
  };
  // A surface the shell opens itself.
  const auto shell = [](const char *request) {
    return [request](C &c, const Arguments &) { c.sendShellRequest(request); };
  };

  // Focus.
  add("focus-column-left", none, ActionNone, [](C &c, auto &) { focusDirection(c, -1, 0); });
  add("focus-column-right", none, ActionNone, [](C &c, auto &) { focusDirection(c, 1, 0); });
  add("focus-window-up", none, ActionNone, [](C &c, auto &) { focusDirection(c, 0, -1); });
  add("focus-window-down", none, ActionNone, [](C &c, auto &) { focusDirection(c, 0, 1); });

  // Columns.
  add("move-column-left", none, column, [](C &c, auto &) {
    c.performLayoutAction("reorder", {{"window", c.focused_->id}, {"direction", -1}});
  });
  add("move-column-right", none, column, [](C &c, auto &) {
    c.performLayoutAction("reorder", {{"window", c.focused_->id}, {"direction", 1}});
  });
  for (const auto &[name, dx] : {std::pair{"consume-or-expel-window-left", -1},
                                 std::pair{"consume-or-expel-window-right", 1}})
    add(name, none, column, [dx](C &c, auto &) {
      c.performLayoutAction("group-direction", {{"window", c.focused_->id},
                                                {"workspace", c.workspace_},
                                                {"dx", dx},
                                                {"dy", 0}});
    });
  add("expel-window-from-column", none, column, [](C &c, auto &) {
    c.performLayoutAction("expel", {{"window", c.focused_->id}});
  });
  add("center-column", none, column, [](C &c, auto &) {
    c.performLayoutAction("center", {{"window", c.focused_->id},
                                     {"area", areaJson(c.workArea())}});
  });
  add(
      "set-column-width", ActionArgument::Word, column,
      [](C &c, const Arguments &arguments) {
        const auto change = widthChange(arguments.front());
        const int area = c.workArea().width();
        const int amount = change->percent
                               ? static_cast<int>(std::lround(area * change->value / 100.0))
                               : change->value;
        const int width = change->relative
                              ? c.focused_->layout.geometry.width() + amount
                              : amount;
        c.performLayoutAction(
            "resize-width",
            {{"window", c.focused_->id},
             {"width", std::clamp(width, WindowTuning::columnMinimumWidth, area)}});
      },
      [](std::string_view word) { return widthChange(word).has_value(); });

  // Windows.
  add("maximize-column", none, ActionNeedsFocus, [](C &c, auto &) {
    c.setMaximized(c.focused_, !c.focused_->state.maximized);
  });
  add("fullscreen-window", none, ActionNeedsFocus, [](C &c, auto &) {
    c.setFullscreen(c.focused_, !c.focused_->state.fullscreen);
  });
  add("close-window", none, ActionNeedsFocus, [](C &c, auto &) { closeClient(c.focused_); });
  add("minimize-window", none, ActionNeedsFocus, [](C &c, auto &) {
    c.focused_->state.minimized = true;
    c.windowLayout_->setMinimized(c.focused_->id, true);
  });
  // Quick tiling, like KDE's: a stacking window fills the half of the screen
  // on that side, a quarter when combined with the other axis, and goes back
  // to its own size when sent the same way again. Tiling moves the focus.
  add(
      "tile-window", ActionArgument::Word, ActionNone,
      [](C &c, const Arguments &arguments) {
        const auto &side = arguments.front();
        const std::uint32_t direction = side == "up"     ? 1u
                                        : side == "down" ? 2u
                                        : side == "left" ? 4u
                                                         : 8u;
        auto *client = c.focused_;
        if (!client || client->state.floating || client->state.fullscreen ||
            !windowTemplateSupportsAction(*c.windowTemplate_, "tile")) {
          focusDirection(c, direction == 4u ? -1 : direction == 8u ? 1 : 0,
                         direction == 1u ? -1 : direction == 2u ? 1 : 0);
          return;
        }
        std::uint32_t tile = 0;
        for (const auto &slot : c.windowLayout_->snapshot(client->state.workspace).columns)
          if (slot.window == static_cast<LayoutWindowId>(client->id))
            tile = static_cast<std::uint32_t>(jsonInt(slot.metadata, "tile"));
        // Toward the other side of the same axis leaves that axis.
        const std::uint32_t axis = direction & 3u ? 3u : 12u;
        const std::uint32_t next = tile == direction ? 0u
                                   : (tile & axis) && !(tile & direction)
                                       ? tile & ~axis
                                       : tile | direction;
        c.performLayoutAction("tile", {{"window", client->id}, {"edges", next}});
      },
      [](std::string_view word) {
        return word == "left" || word == "right" || word == "up" || word == "down";
      });
  // Workspaces. Beyond the last one is the empty workspace after it.
  add("focus-workspace", ActionArgument::Number, ActionNone,
      [](C &c, const Arguments &arguments) {
        c.selectWorkspace(existingWorkspace(c, *number(arguments.front()) - 1));
      });
  add("focus-workspace-up", none, ActionNone,
      [](C &c, auto &) { c.selectWorkspace(existingWorkspace(c, c.workspace_ - 1)); });
  add("focus-workspace-down", none, ActionNone,
      [](C &c, auto &) { c.selectWorkspace(existingWorkspace(c, c.workspace_ + 1)); });
  add("move-window-to-workspace", ActionArgument::Number, ActionNeedsFocus,
      [](C &c, const Arguments &arguments) {
        c.moveClientToWorkspace(c.focused_,
                                existingWorkspace(c, *number(arguments.front()) - 1));
      });
  // The window goes, and the view follows it.
  for (const auto &[name, step] : {std::pair{"move-window-to-workspace-up", -1},
                                   std::pair{"move-window-to-workspace-down", 1}})
    add(name, none, ActionNeedsFocus, [step](C &c, auto &) {
      const int workspace = existingWorkspace(c, c.workspace_ + step);
      c.moveClientToWorkspace(c.focused_, workspace);
      c.selectWorkspace(workspace);
    });
  add("toggle-overview", none, ActionNoRelayout, [](C &c, auto &) { c.toggleOverview(); });
  add("close-overview", none, ActionNoRelayout, [](C &c, auto &) { c.closeOverview(); });

  // Applications.
  add("spawn", ActionArgument::Command, ActionNoRelayout,
      [](C &c, const Arguments &command) {
        std::string error;
        if (!c.launchExternalCommand(command, &error))
          logWarning("spawn %s: %s", command.front().c_str(), error.c_str());
      });
  add(
      "spawn-default", ActionArgument::Word, ActionNoRelayout,
      [](C &c, const Arguments &arguments) {
        c.control({{"method", "launch-default"}, {"value", arguments.front()}});
      },
      [](std::string_view role) { return isDefaultApplicationRole(role); });
  // Without the LunaDash shell the launcher is whatever session.json names.
  add("toggle-launcher", none, ActionNoRelayout, [](C &c, auto &) {
    if (c.session_.shell == SessionConfig::Shell::LunaDash) {
      c.setLauncherVisible(!c.launcherVisible_);
      return;
    }
    std::string error;
    if (!c.session_.launcherCommand.empty() &&
        !c.launchExternalCommand(c.session_.launcherCommand, &error))
      logWarning("The launcher could not start: %s", error.c_str());
  });

  // The shell and the desktop.
  add("open-orbit", none, ActionNoRelayout, shell("orbit-launcher"));
  add("open-control-center", none, ActionNoRelayout, shell("control-center"));
  add("open-clipboard", none, ActionNoRelayout, shell("clipboard"));
  add("open-power-menu", none, ActionNoRelayout, shell("power-menu"));
  add("screenshot", none, ActionNoRelayout, [](C &c, auto &) { c.captureScreen(); });
  add("choose-wallpaper", none, ActionNone, request("choose-wallpaper"));
  add("random-wallpaper", none, ActionNone, request("wallpaper-random"));
  add("toggle-night-light", none, ActionNone, request("eye-care", "toggle"));
}

bool WaylandCompositor::runAction(std::string_view text, std::string *error) {
  const auto call = parseActionCall(text, error);
  if (!call || !checkActionCall(actions_, *call, error))
    return false;
  const auto *action = actions_.find(call->name);
  if ((action->flags & ActionLeavesMaximized) && focused_ &&
      focused_->state.maximized) {
    setMaximized(focused_, false);
    arrange();
  }
  if ((action->flags & ActionNeedsFocus) && !focused_)
    return true;
  action->run(*this, call->arguments);
  if (!(action->flags & ActionNoRelayout)) {
    arrange();
    synchronizeWindowFocus();
  }
  return true;
}

} // namespace LunaDash
