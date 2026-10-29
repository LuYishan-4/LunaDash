# Plugin target reference

`data/plugins/targets.json` is the machine-readable registry. Run `lunadash-create-plugin --list` to inspect targets and supported types, then generate a native effect project with `--target`, `--id` and `--output` (`--language c` for `window-animation`). The generator writes the files itself and the project uses the LunaDash CMake SDK. This is a development API, not a promise of a stable compositor ABI across future SDK major versions.

## Native hooks (`effect`)

Every target is a native compositor hook. The visual Quickshell/OpenGL slots belonged to the former QML shell and were removed with it.

All hooks run synchronously on the host thread. No raw compositor pointers are passed. The JSON envelope and unloading rules are documented in [PLUGINS.md](PLUGINS.md). Configuration and placement hooks run when state changes, rather than invoking third-party code on every rendered frame.

| Target | Response | Context / validation |
| --- | --- | --- |
| `window-animation` | `duration`, `enterOffset`, `focusOpacity`, `exitScale`, `easing` | Complete animation profile, validated against registry bounds; disabling desktop animations still wins |
| `shell-animation` | `{"duration": integer}` | 0–600 milliseconds; shared shell animation duration |
| `window-rules` | `{"workspace": integer, "maximized": boolean}` | `appId`, `title`, `id`; initial map of ordinary windows; existing workspace range only |
| `window-layout` | `{"windows":[{"id":integer,"x":integer,"y":integer,"width":integer,"height":integer}]}` | `workspace`, `area`, `newWindows`, `windowTemplate`, `allowOverlap`; same IDs exactly once, positive sizes, inside work area |

Window layout normally forbids overlap. A native replacement can declare `"windowTemplate":"stacking"` to select the host's persistent stacking strategy; the host publishes `allowOverlap` in the hook context. The host owns per-window size/position, minimization, focus, maximizing/restoring and workspace membership. `newWindows` identifies windows whose initial placement can be customized; later calls preserve manually edited rectangles. Ordinary tiling continues to own its split tree and shared boundaries. Both strategies pass placements through the same hook and update focus/preview geometry from its validated result.

Visual stacking order follows focus-to-front. In stacking mode, taskbar activation restores/focuses a window without automatically zooming it; other windows remain present behind a maximized window. Alt+left-drag moves each window, Shift+Alt+left-drag resizes it independently, and validated xdg-shell titlebar move/edge-resize requests use the same interaction path. Native client popups/dialogs retain host handling. Windows without client decorations can use these shortcuts; this SDK does not add a server-side titlebar protocol.

The [stacking-windows example](../../examples/plugins/stacking-windows/README.md) demonstrates this policy and a configurable initial cascade. It is installed disabled alongside the native fade example when example plugins are enabled.

## Adding a host feature

1. Add a target, category, supported types (`effect`) and built-in setting schema to `targets.json`.
2. Invoke a native filter at the owning domain's state boundary.
3. Define and validate the full response/context contract. Preserve built-in behavior until a valid replacement is ready.
4. Add its contract here and update relevant behavioral tests. The scaffolder, CMake validator and settings categories consume the registry automatically.

A registry entry alone does not implement a feature hook. Do not advertise new targets until their host actually calls/mounts them. Asynchronous services and shader passes need a future host interface; they cannot be implemented by inventing metadata targets.
