# Window interactions (dev)

LunaDash uses scrollable columns with up to eight vertically tiled windows in each column. The taskbar is a flat window list, independent of column membership. This implements LunaDash's own opening and input rules inspired by niri; it does not use niri's compositor or configuration format.

| Input | Result |
| --- | --- |
| New application window | Joins the focused column when it has room; otherwise opens a new column |
| Click a task | Restores and focuses that window; expands its column and increases its row height, preserving other members |
| Alt + left-drag, one active tiled window | Moves the window without resizing |
| Alt + left-drag onto the middle of another window | Exchanges the two slots; slot dimensions remain unchanged |
| Alt + left-drag onto a top/bottom edge | Inserts before/after that member, with a visible insertion marker; maximum eight members |
| Shift + Alt + left-drag | Changes column width and row height; other members share remaining height, with their order preserved |
| Alt+Tab / Alt+Shift+Tab | Opens one horizontal selector and selects next/previous in recent-focus order |
| Release Alt / click a card | Activates the selected window |
| Esc in the selector | Cancels and retains the original focus |
| Super+T / Super+Return | Opens Kitty, or the configured default terminal |

Ordinary application windows stay tiled. Parent-associated dialogs may appear above their parent so modal file pickers and authentication remain usable. Minimized windows reserve membership capacity; restoring one cannot exceed the eight-window limit. Oversized client buffers are clipped to their allotted tile. A task selection gives its row about 70% of the available height, limited by a 48 px minimum for other rows when space permits. Columns scroll horizontally when they exceed the work area.

The selector receives changes through a private JSON file beside the session control socket. Updates occur only on interaction changes, independently of the shell's normal status polling. Its desktop snapshot uses one asynchronous grim process per selection session, capped at 350 ms; failure uses a wallpaper fallback. Snapshots stay in a private temporary directory and are removed when replaced or when the session ends. The selector lists mapped windows across workspaces, including minimized ones. It does not yet provide live window thumbnails.

## Validation

`lunadash-window-layout` runs through CTest and checks eight-member capacity, minimized/restored slots, exact swaps, insertion, non-overlapping geometry, height redistribution, single-window movement, workspace transfer, selector lifecycle and shortcut migration. It needs no running compositor. Qt Quick component checks can run offscreen.

After installing and starting a real session, verify Alt dragging, modifier release, rapid Alt+Tab switching, horizontal selector dragging, popup menus, dialogs, application size hints, and display scaling on the intended hardware. Unit tests and CI builds do not establish frame-rate or multi-monitor interaction correctness. Multi-output layout is still incomplete.
