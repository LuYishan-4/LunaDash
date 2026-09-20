# Window interactions (dev)

LunaDash uses scrollable columns with up to eight vertically tiled windows in each column. The taskbar groups all mapped application windows into workspace capsules, independent of tiling-column membership. The current workspace has a brighter accent fill; others use a darker tint and subdued icons, with no numeric badges. Each app remains individually selectable, including hidden and minimized windows. The strip scrolls horizontally when needed. Defaults are ten workspaces, 33% column width and 6 px gaps; existing customized values are preserved. This implements LunaDash's own opening and input rules inspired by niri; it does not use niri's compositor or configuration format.

| Input | Result |
| --- | --- |
| New application window | Opens a separate column immediately after the focused column |
| Click a task | Switches workspace with an animation and maximizes the selected tiled window; clicking the active maximized task again restores all tiles |
| Alt + left-drag, one active tiled window | Moves the window without resizing |
| Alt + left-drag onto the middle of another window | Exchanges the two slots; slot dimensions remain unchanged |
| Alt + left-drag onto a top/bottom edge | Inserts before/after that member, with a visible insertion marker; maximum eight members |
| Shift + Alt + left-drag | Changes column width and row height; other members share remaining height, with their order preserved |
| Alt+Tab / Alt+Shift+Tab | Opens a 2×5 overview of workspaces 1–10 and selects next/previous |
| Release Alt / click a workspace | Switches to the selected workspace |
| Esc in the overview | Cancels and retains the original focus |
| Super+T / Super+Return | Opens Kitty, or the configured default terminal |

Ordinary application windows stay tiled. Parent-associated dialogs may appear above their parent so modal file pickers and authentication remain usable. Minimized windows reserve membership capacity; restoring one cannot exceed the eight-window limit. Oversized client buffers are clipped to their allotted tile. Maximizing a tiled window fills the work area and temporarily hides all other windows in that workspace, except dialogs belonging to the maximized window. Hidden windows remain in the taskbar and are not minimized or removed from their columns. Restore with Super+F, the application's restore button or another click on the active task: column widths, row heights, ordering and single-window offsets are preserved. Closing or minimizing the maximized window reveals the remaining tiles. Selecting another task transfers maximization to that window. Alt dragging or a layout-editing shortcut restores tiling before changing the layout. Columns scroll horizontally when they exceed the work area.

The overview receives changes through a private JSON file beside the session control socket. Updates are coalesced to at most one per 16 ms, independently of normal status polling. A single asynchronous grim process captures the blurred backdrop, capped at 350 ms; failure uses the wallpaper. Window textures are scaled to at most 320×200 before reading pixels and encoded in a worker. Each of the ten cells represents one workspace and shows all of its visible windows in their actual arrangement. A maximized workspace previews its enlarged window; restoring it brings back the individual tile previews. The cells display windows using app icons until thumbnails arrive or when buffers are unavailable. They are opening-time snapshots, not live video. Snapshot files are private and removed when replaced or on logout.

The overview is a wide, shallow panel near the top of the display, with two rows of five workspaces. Window previews and centered app icons share one panel background; empty workspaces show their number. A single thin selection outline moves between cells. All ten cells remain selectable, including empty workspaces. Selecting a cell beyond a smaller configured workspace count expands that count. Super+0 selects workspace 10; add Shift to move a window there. Arrow keys move through the grid; Up/Down jumps five cells. Workspaces retain their focused window, while individual taskbar clicks enlarge the chosen window. Rounded focus outlines follow the shell theme; client content itself is still rectangular.

## Validation

`lunadash-thumbnail` verifies actual rendered pixel scaling using a software renderer without starting a compositor. `lunadash-window-layout` runs through CTest and checks eight-member capacity, minimized/restored slots, exact swaps, insertion, non-overlapping geometry, height redistribution, single-window movement, workspace transfer, maximize/hide/restore without changing saved tile geometry, selector lifecycle and shortcut migration. It needs no running compositor. Qt Quick component checks can run offscreen.

After installing and starting a real session, verify Alt dragging, modifier release, rapid Alt+Tab switching, workspace-grid selection, popup menus, dialogs, application size hints, and display scaling on the intended hardware. Unit tests and CI builds do not establish frame-rate or multi-monitor interaction correctness. Multi-output layout is still incomplete.
