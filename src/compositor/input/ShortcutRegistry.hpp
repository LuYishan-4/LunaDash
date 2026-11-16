#pragma once

#include "core/templates/NamedRegistry.hpp"

#include <functional>
#include <string_view>

namespace LunaDash {

class WaylandCompositor;

enum ShortcutFlags : unsigned {
  ShortcutNone = 0,
  // Skipped (the relayout still runs) without a focused window.
  ShortcutNeedsFocus = 1u << 0,
  // Column operations restore a maximized focused window first.
  ShortcutLeavesMaximized = 1u << 1,
  // The action lays out or focuses on its own; no relayout afterwards.
  ShortcutNoRelayout = 1u << 2,
};

// A keyboard shortcut action, named as ShortcutSettings binds it.
struct ShortcutAction {
  unsigned flags = ShortcutNone;
  std::function<void(WaylandCompositor &, std::string_view action)> run;
};

// An action family named <prefix><N> (workspace3, moveToWorkspace1). The
// handler receives N, or 0 when the suffix is not a number.
struct NumberedShortcutAction {
  std::function<void(WaylandCompositor &, int number)> run;
};

// Shortcut actions the compositor can execute. Built-ins are registered by
// ShortcutActions.cpp; handleShortcut() only looks names up.
struct ShortcutRegistry {
  Templates::NamedRegistry<ShortcutAction> actions;
  // Keyed by prefix.
  Templates::NamedRegistry<NumberedShortcutAction> numbered;
};

} // namespace LunaDash
