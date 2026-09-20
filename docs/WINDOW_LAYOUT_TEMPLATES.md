# Window layout templates

`src/compositor/layout/WindowLayout.hpp` is the contract between the compositor and a layout implementation. `LayoutTemplates.cpp` registers the known modes and creates implementations. The compositor holds a `WindowLayout` rather than depending directly on `TilingLayout`.

| Template | Available | Implementation |
| --- | --- | --- |
| `tiling` | Yes | `src/compositor/tiling/TilingLayout.cpp`; bounded split tiles and grouped rows |
| `stacking` | Reserved | No implementation yet; the factory returns null |

There is intentionally no user-facing mode switch yet. Requesting an unimplemented template must leave the existing layout active. The reserved entry is an extension point, not an enabled stacking mode.

To add a mode:

1. Create its interface and implementation together in a lowercase source domain, implement `WindowLayout`, and explicitly add the source to CMake.
2. Preserve stable window/workspace IDs across insertion, removal, minimization, workspace movement and focus. `WindowPlacement` supplies geometry and visibility; row/group fields may remain unused where they do not apply. Unsupported grouping or movement operations return false.
3. Keep normal layout state separate from `presentation()`. Maximizing changes presentation, while restoring recovers the saved arrangement. Do not resize clients, create scene nodes, poll input or start animations inside the layout.
4. Add the factory implementation and mark the descriptor implemented. A future mode-switch controller must transfer the current window inventory and focus before replacing the active instance; no such controller is enabled in this version.
5. Verify lifecycle, focus, bounds, workspace transfers and maximize/restore using the common interface. Mode-specific overlap rules belong to the implementation's tests.

The compositor's scene animation code handles opening, closing, layout changes, maximizing and restoring. Interactive resizing follows the pointer directly. At drop completion the input controller releases its interactive override **before** arranging, so the moved window and affected neighbors animate into their final slots. Implementations should return final target rectangles and leave interpolation to that shared path.
