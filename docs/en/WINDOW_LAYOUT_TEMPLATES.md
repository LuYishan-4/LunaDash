# Window layout templates

`src/compositor/layout/WindowLayout.hpp` is the contract between the compositor and a layout implementation. `src/compositor/window/WindowTemplate.cpp` registers the known modes, their settings and their factories. The compositor holds a `WindowLayout` rather than depending directly on `TilingLayout`.

| Template | Available | Implementation |
| --- | --- | --- |
| `tiling` | Yes | `src/compositor/tiling/TilingLayout.cpp`; bounded split tiles and grouped rows |
| `stacking` | Via plugin | `src/compositor/layout/FreeformLayout.cpp`; retained overlapping rectangles |

Bounded tiling is the default. Enabling a native stacking replacement in Desktop extensions migrates the window inventory and focus live. Disabling it restores tiling.

## Focused split layout

The default arrangement uses the existing bounded split tree. The first two windows form a left/right pair, keeping the first window on the right. New windows then divide the focused tile, alternating top/bottom and left/right at each level. Focus a tile before opening an application to choose which part of the desktop it divides. Other branches retain their geometry. When a focused tile has too little space to split, insertion uses the largest active tile. Grouped windows remain rows within their existing leaf; grouping, shared-boundary resize, minimize/restore and maximize/restore continue to use the same template.

For the six-window reference arrangement: open three windows, focus the upper-left window, then open three more. The first window stays in the full-height right half; the lower-left tile remains intact while the upper-left branch splits into progressively smaller panes. Ordinary focus changes never rearrange tiles.

Settings > Windows and workspaces > Window layout exposes:

| Setting | Default | Behavior |
| --- | --- | --- |
| `splitTarget` | `focused` | Split the active tile with alternating axes; `largest` selects the largest tile and divides its longer dimension. |
| `firstWindowSide` | `right` | Keep the original window on the right of the first pair; `left` places it on the left. |
| `gap` | inherited desktop gap, initially 12 | Space between adjacent tiles. |
| `defaultWidth` | 1120 | Initial width for a lone window when it does not supply a preferred size. |

The first pair always uses left/right placement. Split policy and first-window-side changes apply to future insertions, workspace moves and group expulsions; saved tile arrangements are retained. Existing explicit settings remain authoritative. The fields are validated and persisted by the existing `layout:tiling` settings target under `windowLayout/tiling/`; no new configuration file, Niri parser or layout backend is added. Selecting `largest` and `left` restores the earlier insertion policy for a typical landscape screen.

The existing layout CI target includes the reference arrangement, policy switching, destination focus, minimized-window recovery, small-screen bounds and maximize/restore regressions. These changes were not built or tested locally at the user's request; CI results and real-session visual verification must be reported separately.

## Extending a template

To add a mode:

1. Create its interface and implementation together in a lowercase source domain, implement `WindowLayout`, and explicitly add the source to CMake.
2. Preserve stable window/workspace IDs across insertion, removal, minimization, workspace movement and focus. `WindowPlacement` supplies geometry and visibility; row/group fields may remain unused where they do not apply. Unsupported grouping or movement operations return false.
3. Keep normal layout state separate from `presentation()`. Maximizing changes presentation, while restoring recovers the saved arrangement. Do not resize clients, create scene nodes, poll input or start animations inside the layout.
4. Add the factory implementation and mark the descriptor implemented. The compositor transfers the current window inventory and focus when a validated strategy plugin changes mode.
5. Verify lifecycle, focus, bounds, workspace transfers and maximize/restore using the common interface. Mode-specific overlap rules belong to the implementation's tests.

The compositor's scene animation code handles opening, closing, layout changes, maximizing and restoring. Interactive resizing follows the pointer directly. At drop completion the input controller releases its interactive override **before** arranging, so the moved window and affected neighbors animate into their final slots. Implementations should return final target rectangles and leave interpolation to that shared path.

## SDK 2 strategy plugins

Stacking is now implemented behind the same interface and selected by a validated, enabled `window-layout` replacement declaring `windowTemplate: stacking`. It is no longer a null factory placeholder. The default remains bounded tiling. Both strategies support a validated placement filter; the host retains membership and interaction state. The actual stacking/cascade policy ships as the SDK 2 package in [`data/plugins/stacking-windows`](../../data/plugins/stacking-windows/), rather than as an SDK template. See [the target contract](PLUGIN_TARGETS.md). Enabling/disabling or rebuilding hooks applies live between callbacks.
