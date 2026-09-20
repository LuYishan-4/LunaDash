# Stacking windows example

A native SDK 2 plugin that selects conventional overlapping windows with independent movement and sizing. New windows start in a configurable cascade. Existing positions and sizes survive focus changes, closing neighbors, minimization and maximize/restore.

The standard LunaDash build includes this plugin **disabled**. Open Settings → Shell modules → Desktop extensions → Windows → Window layout, enable **Stacking windows**, keep **Plugin only**, and save. The layout changes live; no reboot or logout is required. Disabling it restores built-in tiling. Existing windows are migrated between strategies; tiling split weights are reset when changing strategy.

To build the example separately:

```sh
cmake -S examples/plugins/stacking-windows -B build-stacking -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-stacking
cmake --install build-stacking
```

For a source build SDK, add `-DLunaDashPlugin_DIR=/absolute/path/to/lunadash-build/sdk-build` to the configure command. Rebuilding/reinstalling reloads the plugin live.

- Click or select a taskbar entry to focus and raise its window.
- Drag a client-drawn titlebar, or hold Alt and drag, to move.
- Drag a supported client edge, or hold Shift+Alt and drag, to resize only that window.
- Meta+F maximizes/restores the selected window; other windows remain behind it.
- Change `cascadeStep` in the plugin settings to position subsequently opened windows.

`StackingWindows.cpp` implements one placement hook. `metadata.json` selects `effect`, `window-layout`, `replace`, and `stacking`; CMake embeds the manifest and generates the ABI export. The host owns safe geometry bounds, input serial validation, retained positions and animation. Native plugins execute in the compositor and are not sandboxed.
