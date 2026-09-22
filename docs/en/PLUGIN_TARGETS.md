# Plugin target reference

`data/plugins/targets.json` is the machine-readable registry. Run `lunadash-create-plugin --list` to inspect targets and supported types, then generate a starter with `--type`, `--target`, `--id` and `--output`. Templates are installed under `share/lunadash/plugin-sdk/templates`; all use the LunaDash CMake SDK. This is a development API, not a promise of a stable compositor ABI across future SDK major versions.

## Visual targets (`quickshell` / `opengl`)

Every visual slot has its host's dimensions. The host retains layer placement, input region and open/close lifecycle; the plugin supplies content. Common context: `target`, `source` (built-in item), `builtinSettings`, `mode`, `reportError(message)`. Module surfaces also provide `module`, `moduleId`, `style`, `config`, `opened`. Built-in `opacity` affects that target's original content; plugin settings are independent.

| Target | Original feature / extra context |
| --- | --- |
| `panel` | Taskbar surface, workspace controls and tray |
| `taskbar-windows` | Window list inside the taskbar; `groups` |
| `wallpaper` | Desktop wallpaper surface and context menu input |
| `wallpaper-transition` | Shared desktop/settings wallpaper animation; `imageSource`, `previous`, `incoming`, `progress`, `transitioning` |
| `launcher` | App launcher |
| `overview` | Dashboard surface |
| `media` | Media view; `controller`, `media`, including actions and player selection |
| `calendar` | Calendar popup |
| `desktop-menu` | Desktop context menu |
| `audio`, `network`, `devices` | Audio/network/removable-device popup content |
| `session` | Logout/session controls |
| `image-picker` | Local image chooser; `picker` controller, `purpose` |
| `settings` | Settings window content; Plugins and Shell modules bypass replacement for recovery |
| `setup` | Welcome view |
| `compatibility` | Compatibility launcher |
| `feedback`, `notifications`, `screenshot` | Messages, notifications, screenshot feedback |
| `workspace-transition`, `startup` | Workspace and startup animation visuals |
| `window-decoration` | Window border overlay; `interaction` |
| `window-switcher` | Alt+Tab workspace/window overview; `interaction` |
| `tiling-hint` | Drop insertion indicator |
| `blur` | Switcher backdrop blur; `image` (unblurred snapshot), `reveal` |
| `settings.*` | Individual settings pages; `page` and plugin `implicitHeight` control scrolling |

Settings-page targets: `general`, `appearance`, `windows`, `shortcuts`, `display`, `input`, `input-method`, `sound`, `network`, `bluetooth`, `power`, `applications`, `privacy`, `system`, `devices`, `about`, `dashboard`. `settings.modules` and `settings.plugins` are reserved for built-in recovery. Replacing a settings page changes its interface; the host continues to own validated display/input/power/system operations.

`desktop-widgets` accepts **Quickshell only**. Its root item has no surface; it can create its own PanelWindows. It is hosted independently of the wallpaper and unloaded during shutdown. Use ordinary item plugins for the other slots so the host can preserve input, visibility and fallback behavior.

## Native hooks (`effect`)

All hooks run synchronously on the host thread. No raw compositor pointers are passed. The JSON envelope and unloading rules are documented in [PLUGINS.md](PLUGINS.md). Configuration and placement hooks run when state changes, rather than invoking third-party code on every rendered frame.

| Target | Response | Context / validation |
| --- | --- | --- |
| `window-animation` | `duration`, `enterOffset`, `focusOpacity`, `exitScale`, `easing` | Complete animation profile, validated against registry bounds; disabling desktop animations still wins |
| `shell-animation` | `{"duration": integer}` | 0–600 milliseconds; shared QML animation duration |
| `window-rules` | `{"workspace": integer, "maximized": boolean}` | `appId`, `title`, `id`; initial map of ordinary windows; existing workspace range only |
| `window-layout` | `{"windows":[{"id":integer,"x":integer,"y":integer,"width":integer,"height":integer}]}` | `workspace`, `area`, `newWindows`, `windowTemplate`, `allowOverlap`; same IDs exactly once, positive sizes, inside work area |

Window layout normally forbids overlap. A native replacement can declare `"windowTemplate":"stacking"` to select the host's persistent stacking strategy; the host publishes `allowOverlap` in the hook context. The host owns per-window size/position, minimization, focus, maximizing/restoring and workspace membership. `newWindows` identifies windows whose initial placement can be customized; later calls preserve manually edited rectangles. Ordinary tiling continues to own its split tree and shared boundaries. Both strategies pass placements through the same hook and update focus/preview geometry from its validated result.

Visual stacking order follows focus-to-front. In stacking mode, taskbar activation restores/focuses a window without automatically zooming it; other windows remain present behind a maximized window. Alt+left-drag moves each window, Shift+Alt+left-drag resizes it independently, and validated xdg-shell titlebar move/edge-resize requests use the same interaction path. Native client popups/dialogs retain host handling. Windows without client decorations can use these shortcuts; this SDK does not add a server-side titlebar protocol.

The [stacking-windows example](../../examples/plugins/stacking-windows/README.md) demonstrates this policy and a configurable initial cascade. It is installed disabled alongside the native fade example when example plugins are enabled. The four generic templates cover C, C++, Quickshell and OpenGL.

## Adding a host feature

1. Add a target, category, supported types and built-in setting schema to `targets.json`.
2. Put an `ExtensionSlot` at the feature's QML content boundary, or invoke a native filter at the owning domain's state boundary.
3. Define and validate the full response/context contract. Preserve built-in behavior until a valid replacement is ready.
4. Add its contract here and update relevant behavioral tests. The scaffolder, CMake validator and settings categories consume the registry automatically.

A registry entry alone does not implement a feature hook. Do not advertise new targets until their host actually calls/mounts them. Asynchronous services and application-buffer shader passes need a future host interface; they cannot be implemented by inventing metadata targets.


## 1.0.1a desktop-widget rule

`desktop-widgets` uses `selection: multiple`. The host embeds every enabled QML item into the wallpaper Background surface. Multiple widgets may coexist, and application windows render above them.
