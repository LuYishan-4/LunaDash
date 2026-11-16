#pragma once

#include "compositor/input/ShortcutRegistry.hpp"

namespace LunaDash {

class WaylandCompositor;

// The compositor's built-in keyboard shortcut actions.
struct ShortcutActions {
  static void registerBuiltins(ShortcutRegistry &registry);

private:
  static void focusDirection(WaylandCompositor &c, int dx, int dy);
  static void reorder(WaylandCompositor &c, int direction);
  static void groupDirection(WaylandCompositor &c, int dx);
  static void resizeColumn(WaylandCompositor &c, int width);
  static void toggleLauncher(WaylandCompositor &c);
};

} // namespace LunaDash
