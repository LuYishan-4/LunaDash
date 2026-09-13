# Native plugins

LuDash uses metadata inspired by KDE's `KPlugin` structure. **It does not implement KWin's ABI and cannot load KWin plugins.**

Place a plugin in `~/.local/share/ludash/plugins/<id>/` or `$prefix/share/ludash/plugins/<id>/`. Development builds also scan `plugins/` next to the executable. Each directory contains `metadata.json` and a shared library.

```json
{
  "KPlugin": {
    "Id": "org.example.effect",
    "Name": "Example effect",
    "Description": "An example window effect",
    "Version": "1.0.0",
    "License": "GPL-3.0-only",
    "EnabledByDefault": false
  },
  "LuDash": {
    "ApiVersion": 1,
    "Type": "WindowEffect",
    "Library": "libexample-effect.so"
  }
}
```

Derive the C++ class from `QObject` and `LuDash::CompositorPlugin`. Implement `windowOpened(QQuickItem*)` and `windowFocused(QQuickItem*)`, embed matching metadata using `Q_PLUGIN_METADATA` and declare the interface with `Q_INTERFACES`. Use `QPointer<QQuickItem>` if retaining a window reference; do not dereference destroyed windows.

The complete example is in `include/LuDash/fade_plugin/FadePlugin.h`, `src/fade_plugin/FadePlugin.cpp` and `data/plugins/fade/metadata.json`. It builds into `build/plugins/org.ludash.fade/`. Enable it in the plugin manager and restart the compositor to load it.

The loader validates JSON size, ID, API version, type, canonical library path, Qt plugin IID and embedded ID. Native plugins are disabled by default. Metadata `EnabledByDefault` cannot override explicit user settings. There is no signing or sandbox: enabling a native plugin trusts its code inside the compositor process, where it can access the session or crash it.

Reference: [KDE metadata structure](https://develop.kde.org/docs/plasma/widget/setup/).
