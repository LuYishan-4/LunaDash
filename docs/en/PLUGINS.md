# Desktop plugins

LunaDash Plugin SDK 2 provides feature hooks, visual slots and shared metadata for local plugins. The built-in tiling desktop stays the default. Plugins can replace a feature or run alongside it. All native effects ship disabled.

## Types and targets

| `type` | Implementation | Runtime |
| --- | --- | --- |
| `effect` | C11 or C++20, compiled with the SDK | Versioned C ABI with JSON hook requests and validated results |
| `quickshell` | QML/JavaScript | An `Item` inside the selected shell feature; desktop widgets may own windows |
| `opengl` | GLSL `.vert` and `.frag` | SDK-baked shader packages in a Qt Quick visual slot |

`target` names the feature being replaced, independently of the implementation language. [The target reference](PLUGIN_TARGETS.md) lists every registered hook, visual slot and settings-page slot. The registry is `data/plugins/targets.json`; the SDK and runtime use the same registry. A package may keep the legacy single-target fields or declare a `targets` array. Each target declares its own `type`, `target`, `mode`, entry/shaders and settings schema, while the package keeps one shared identity and version.

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
    "enabled": {"type": "boolean", "control": "toggle", "default": true},
    "mode": {"type": "string", "control": "select", "default": "soft",
             "enum": ["soft", "strong"]},
    "amount": {"type": "integer", "control": "number", "default": 2},
    "strength": {"type": "number", "control": "slider", "default": 0.5,
                 "minimum": 0, "maximum": 1}
  }
}
```

`mode` is `replace` (Plugin only) or `augment` (Built-in and plugin). Users choose the effective mode in Settings → Plugins → Desktop extensions. Targets marked `selection: "single"` allow only one enabled plugin implementation at a time. When a user enables another implementation in Settings, LunaDash shows the conflicting plugin(s); after confirmation it disables the old target configuration and enables the new one. The backend enforces the same rule for manually edited JSON. `desktop-widgets` remains multi-select. Replacement mode is still exclusive even on composable targets. Layouts that request `windowTemplate: stacking` must be replacements: two layout owners cannot simultaneously place the same windows.

For a multi-target package, runtime configuration is nested by target:

```json
{
  "schemaVersion": 1,
  "builtins": {},
  "plugins": {
    "org.example.behavior": {
      "enabled": true,
      "targets": {
        "panel": {"enabled": true, "mode": "replace", "settings": {}},
        "window-rules": {"enabled": true, "mode": "replace", "settings": {}}
      }
    }
  }
}
```

Plugin settings are rendered automatically through the shared Settings API. SDK 2 plugins may expose only four stable controls: **Yes/No** (`toggle`), **drop-down** (`select`), **numeric input** (`number`) and **numeric slider** (`slider`). Free-form text and array controls remain host/module-only and are rejected for plugins. Unknown settings and invalid values are rejected both by the SDK and again by the runtime before loading or saving. The SDK generates `metadata.json` and `.lunadash-sdk.json`; the runtime checks the receipt against the manifest. Native libraries also embed that manifest and export the SDK ABI. These checks detect missing/stale builds; they are **not signatures or a sandbox**.

## Hooks and runtime loading

Enabling, disabling and changing settings do not require rebooting or logging out. Plugin packages are used directly from their installed directories; there is no private revision copy, polling fingerprint or background package watcher. Native libraries stay loaded while enabled, so disable a native plugin before replacing its binary and then enable it again. A failed direct load falls back to the built-in feature; Retry plugin clears the load error and tries the installed package again.

Native SDK 2 deliberately uses synchronous, stateless hooks. Authors implement `ludash_plugin_process(request, response, capacity)`; the SDK generates `ludash_plugin_entry_v2` and embeds metadata. Requests contain `target`, `mode`, `context`, `builtin`, `current`, and `settings`. Return a complete JSON object and its byte length, or a negative value on failure. Replacement hooks get an empty `current`; `builtin` remains available as a starting point. Additions receive the previous valid result. Invalid responses are quarantined until settings/code change or the user retries.

Do not retain host pointers, start background threads, register process-wide callbacks, run nested event loops or block inside a hook. A native library remains loaded while its plugin is enabled and is unloaded when the plugin is disabled, removed, or replaced by a changed manifest. Native code is unrestricted and a crash or hang can still take down the compositor; metadata validation does not isolate it. SDK 2 does not expose scene pointers or a general asynchronous service ABI.

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

## Community registry and Plugin Store

Community plugins are published through [LunaDash-Plugins](https://github.com/LuYishan-4/LunaDash-Plugins). A submission lives in `plugins/<id>/`, is validated against the SDK 2 target, manifest and settings contracts, and must pass registry CI and maintainer review before it can appear in the generated catalogue. The companion Astro site browses the same reviewed registry.

Settings → Plugins → Store reads the reviewed registry from [LunaDash-Plugins](https://github.com/LuYishan-4/LunaDash-Plugins). The runtime fetches `https://raw.githubusercontent.com/LuYishan-4/LunaDash-Plugins/main/index.json` over HTTPS and validates the catalogue identity, target/type contract, tags and remote URLs before exposing entries to QML. A bundled copy of the same registry is used when the network catalogue is unavailable. Set `LUNADASH_PLUGIN_CATALOG_URL` to another HTTPS index for development, or to `off` to disable network refresh.

The Store is the discovery source; the existing SDK/CMake installation path is unchanged. Source and marketplace links come from the registry, while installed packages are still validated from their local `metadata.json` and SDK receipt before they can run. Native plugins remain unrestricted code and are never enabled merely because they appear in the Store.

Metadata and QML run with user permissions. Protocol/session ownership, authentication, system services and update installation are host responsibilities, not replaceable native services in SDK 2.
