# Window layout templates

`src/compositor/layout/WindowLayout.hpp` is the contract between the compositor and a layout implementation. `LayoutTemplates.cpp` registers the known modes and creates implementations. The compositor holds a `WindowLayout` rather than depending directly on `TilingLayout`.

| Template | Available | Implementation |
| --- | --- | --- |
| `tiling` | Yes | `src/compositor/tiling/TilingLayout.cpp`; bounded split tiles and grouped rows |
| `stacking` | Via plugin | `src/compositor/stacking/StackingLayout.cpp`; retained overlapping rectangles |

Bounded tiling is the default. Enabling a native stacking replacement in Desktop extensions migrates the window inventory and focus live. Disabling it restores tiling.

To add a mode:

1. Create its interface and implementation together in a lowercase source domain, implement `WindowLayout`, and explicitly add the source to CMake.
2. Preserve stable window/workspace IDs across insertion, removal, minimization, workspace movement and focus. `WindowPlacement` supplies geometry and visibility; row/group fields may remain unused where they do not apply. Unsupported grouping or movement operations return false.
3. Keep normal layout state separate from `presentation()`. Maximizing changes presentation, while restoring recovers the saved arrangement. Do not resize clients, create scene nodes, poll input or start animations inside the layout.
4. Add the factory implementation and mark the descriptor implemented. The compositor transfers the current window inventory and focus when a validated strategy plugin changes mode.
5. Verify lifecycle, focus, bounds, workspace transfers and maximize/restore using the common interface. Mode-specific overlap rules belong to the implementation's tests.

The compositor's scene animation code handles opening, closing, layout changes, maximizing and restoring. Interactive resizing follows the pointer directly. At drop completion the input controller releases its interactive override **before** arranging, so the moved window and affected neighbors animate into their final slots. Implementations should return final target rectangles and leave interpolation to that shared path.

## SDK 2 strategy plugins

Stacking is now implemented behind the same interface and selected by a validated, enabled `window-layout` replacement declaring `layoutMode: stacking`. It is no longer a null factory placeholder. The default remains bounded tiling. Both strategies support a validated placement filter; the host retains membership and interaction state. See [the target contract](PLUGIN_TARGETS.md) and [the stacking plugin](../examples/plugins/stacking-windows/README.md). Enabling/disabling or rebuilding hooks applies live between callbacks.
