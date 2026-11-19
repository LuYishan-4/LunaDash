#pragma once

#include "core/templates/NamedRegistry.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {

class WaylandCompositor;

// What an action takes after its name: nothing, a number ("focus-workspace
// 3"), one word ("spawn-default terminal", "set-column-width +10%") or a
// command line ("spawn kitty -e htop").
enum class ActionArgument { None, Number, Word, Command };

enum ActionFlags : unsigned {
  ActionNone = 0,
  // Does nothing without a focused window.
  ActionNeedsFocus = 1u << 0,
  // A column operation: a maximized focused window is restored first.
  ActionLeavesMaximized = 1u << 1,
  // The action lays out or focuses on its own; no relayout afterwards.
  ActionNoRelayout = 1u << 2,
};

struct CompositorAction {
  ActionArgument argument = ActionArgument::None;
  unsigned flags = ActionNone;
  std::function<void(WaylandCompositor &, const std::vector<std::string> &arguments)> run;
  // For a Word argument: whether the word is one the action takes.
  std::function<bool(std::string_view word)> accepts;
};

// The actions keys are bound to and `lunadashctl action` runs, by name.
using ActionRegistry = Templates::NamedRegistry<CompositorAction>;

// "spawn kitty -e 'a b'": the name and its arguments; quotes group words.
struct ActionCall {
  std::string name;
  std::vector<std::string> arguments;
};
std::optional<ActionCall> parseActionCall(std::string_view text, std::string *error);
// Whether the call names a registered action, with the arguments it takes.
bool checkActionCall(const ActionRegistry &registry, const ActionCall &call,
                     std::string *error);

struct CompositorActions {
  static void registerBuiltins(ActionRegistry &registry);

private:
  static void focusDirection(WaylandCompositor &c, int dx, int dy);
  // `workspace`, kept to the workspaces there are.
  static int existingWorkspace(WaylandCompositor &c, int workspace);
};

} // namespace LunaDash
