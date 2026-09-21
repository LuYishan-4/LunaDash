# Desktop plugins

LunaDash Plugin SDK 2 provides feature hooks, visual slots and shared metadata for local plugins. The built-in tiling desktop stays the default. Plugins can replace a feature or run alongside it. All native effects ship disabled.

## Types and targets

| `type` | Implementation | Runtime |
| --- | --- | --- |
| `effect` | C11 or C++20, compiled with the SDK | Versioned C ABI with JSON hook requests and validated results |
| `quickshell` | QML/JavaScript | An `Item` inside the selected shell feature; desktop widgets may own windows |
| `opengl` | GLSL `.vert` and `.frag` | SDK-baked shader packages in a Qt Quick visual slot |

`target` names the feature being replaced, independently of the implementation language. [The target reference](PLUGIN_TARGETS.md) lists every registered hook, visual slot and settings-page slot. The registry is `data/plugins/targets.json`; the SDK and runtime use the same registry. A plugin has one target; a larger collection can ship several independently selectable plugins.

## Create and build

The SDK is installed with LunaDash. Python 3 and CMake are required. Native C++ examples also use Qt Core; shader plugins additionally require Qt Shader Tools (`qt6-shadertools` on Arch). QML plugins also go through CMake so they receive validated metadata and a build receipt.

```sh
lunadash-create-plugin --list
lunadash-create-plugin --type quickshell --target panel \
  --id org.example.panel --output my-panel
cmake -S my-panel -B my-panel/build -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build my-panel/build
cmake --install my-panel/build
```

From a checkout, use `scripts/create-plugin.py`. To build against an uninstalled LunaDash build, pass `-DLunaDashPlugin_DIR=/absolute/path/to/build/sdk-build`. To test a relocated installation, set `CMAKE_PREFIX_PATH` to its installation prefix.

Every plugin project calls:

```cmake
find_package(LunaDashPlugin 2 CONFIG REQUIRED)
lunadash_add_plugin(my_plugin METADATA "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json"
    SOURCES Effect.cpp) # SOURCES is required only for native effects
```

Additional QML/JS/assets can be listed explicitly with `FILES`; the SDK installs these in the plugin directory. Filenames in metadata are local leaf filenames. Do not hand-copy only the source directory into the plugin search path: the SDK must build/install the complete package.

## Metadata

```json
{
  "schemaVersion": 2,
  "sdk": {"name": "LunaDash", "apiVersion": 2},
  "id": "org.example.panel",
  "name": "My panel",
  "description": "A custom taskbar.",
  "version": "1.0.0",
  "author": "Your name",
  "type": "quickshell",
  "target": "panel",
  "mode": "replace",
  "entry": "Main.qml",
  "enabledByDefault": false,
  "settings": {
    "text": {"type": "string", "default": "Hello"}
  }
}
```

`mode` is `replace` (Plugin only) or `augment` (Built-in and plugin). Users choose the effective mode in Settings → Plugins → Desktop extensions. Only one replacement is allowed per target; additions run after it, ordered by plugin ID. A failed replacement leaves the original feature available. Layouts that request `windowTemplate: stacking` must be replacements: two layout owners cannot simultaneously place the same windows. `lunadash-create-plugin --type effect --target window-layout ...` now starts from the generic native effect template. The shipped stacking/cascade implementation is a real SDK 2 package under `data/plugins/stacking-windows`, while the compositor core retains only generic freeform geometry state for stacking-mode interaction.

The settings schema supports `boolean`, `string`, `number`, `integer`, `default`, numeric `minimum`/`maximum`, and `enum`. Unknown settings and invalid values are rejected before saving. The SDK generates `metadata.json` and `.lunadash-sdk.json`; the runtime checks the receipt against the manifest. Native libraries also embed that manifest and export the SDK ABI. These checks detect missing/stale builds; they are **not signatures or a sandbox**.

## Hooks and hot reload

Enabling, disabling, changing settings and rebuilding a plugin do not require rebooting or logging out. The host checks local packages approximately once per second and reloads between hook calls. Each revision is staged into a private directory: a rebuild cannot overwrite executing native code, and QML/helper/shader URLs change together. Recent QML revisions remain available while asynchronous loads settle. A failed or incomplete revision falls back to the built-in feature; the settings page shows its error and offers Retry plugin.

Native SDK 2 deliberately uses synchronous, stateless hooks. Authors implement `ludash_plugin_process(request, response, capacity)`; the SDK generates `ludash_plugin_entry_v2` and embeds metadata. Requests contain `target`, `mode`, `context`, `builtin`, `current`, and `settings`. Return a complete JSON object and its byte length, or a negative value on failure. Replacement hooks get an empty `current`; `builtin` remains available as a starting point. Additions receive the previous valid result. Invalid responses are quarantined until settings/code change or the user retries.

Do not retain host pointers, start background threads, register process-wide callbacks, run nested event loops or block inside a hook. Libraries are unloaded after the current callback returns. Native code is unrestricted and a crash or hang can still take down the compositor; metadata validation does not isolate it. SDK 2 does not expose scene pointers or a general asynchronous service ABI.

Quickshell entry points declare `required property var shell`, `settings` and `context`. These are supplied **before** component construction. The context includes the feature's state and `source`, the built-in visual item. Use `shell.command`, `shell.launch` and `shell.openUrl` for the existing desktop actions. See the target reference for additional context fields. Plugins can expose `pluginReady: false` until ready; the replacement stays hidden until then. The Plugins and Shell modules settings pages always remain built-in as recovery routes.

## OpenGL shader packages

Use `type: opengl`, a supported visual `target`, and:

```json
"shaders": {"vertex": "Effect.vert", "fragment": "Effect.frag"}
```

The SDK compiles both files with `qsb`; the installed package retains the GLSL and includes `.qsb` files. The shared uniform block contains `qt_Matrix`, `qt_Opacity`, `strength`, `resolution`, and `parameters` (a vec4 mapped from numeric settings `parameter0`–`parameter3`). Binding 1 is the `source` sampler. Keep the uniform block layout identical in both stages and output premultiplied colors. The `templates/plugins/opengl` example is a complete reference.

Replacement shaders suppress the built-in draw only after successful shader creation, while keeping its texture as input. Additions composite on top in ID order; each gets the built-in texture, not the previous shader's framebuffer. Software rendering does not run shaders and retains the built-in visual. The shader slot is in the Quickshell scene: it can affect wallpaper, shell surfaces and the switcher background, **not arbitrary application buffers or the wlroots output framebuffer**. Raw compositor OpenGL passes remain under `src/compositor/renderer/opengl/` and are a separate renderer integration boundary.

Qt references: [ShaderEffect](https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html), [qsb](https://doc.qt.io/qt-6/qtshadertools-qsb.html).

## Settings and recovery

Settings → Plugins groups features into Desktop, Windows, Animation, Effects, System, Feedback and Settings pages. Each target shows its built-in options and installed plugins, with separate plugin options, enabled state and composition mode. Shell layout controls and `shell-modules.json` remain in Settings → Shell modules. Advanced JSON edits `~/.config/LuDash/extensions.json`; saving validates the complete document (maximum 24 KiB) and writes it atomically. Native defaults such as duration/gap use `-1` to inherit the existing desktop preference.

```json
{
  "schemaVersion": 1,
  "builtins": {"window-animation": {"duration": 260}},
  "plugins": {
    "org.lunadash.stacking-windows": {
      "enabled": true, "mode": "replace", "settings": {"cascadeStep": 32}
    }
  }
}
```

The corresponding control commands are `lunadashctl extension-save '<JSON>'` and `lunadashctl open-settings plugins`. To restore all extension defaults, save `{"schemaVersion":1,"builtins":{},"plugins":{}}` and disable legacy plugin preferences if any were previously enabled. To explicitly turn off an individual installed plugin, keep its entry with `enabled: false`.

Discovery prefers the user's data directory, then system data directories: `lunadash/plugins`, legacy `lunadash/shell/plugins`, legacy `ludash/plugins`; an executable-adjacent `plugins` directory supports development builds. Legacy schema-1 QML widgets continue as desktop widgets. The old Qt Quick native effect ABI is rejected with a rebuild message; it never drove the active wlroots scene. Existing trusted custom module QML remains a migration path, but new plugin packages use SDK 2.

The plugin store remains unimplemented. Metadata and QML run with user permissions. Protocol/session ownership, authentication, system services and update installation are host responsibilities, not replaceable native services in SDK 2.
