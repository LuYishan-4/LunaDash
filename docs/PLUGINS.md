# Plugins

LunaDash supports two plugin types:

- `qml` for shell/UI extensions loaded by Quickshell.
- `effect` for native C++ compositor effects loaded through `QPluginLoader`.

The plugin manager has **Installed** and **Store** tabs. The Store tab is currently a placeholder and does not download plugins yet.

## LunaDash manifest schema

New plugins use `metadata.json` (also referred to as the plugin manifest):

```json
{
  "schemaVersion": 1,
  "id": "org.example.clock",
  "name": "Example Clock",
  "name[zh_TW]": "\u7bc4\u4f8b\u6642\u9418",
  "description": "A desktop clock plugin.",
  "description[zh_TW]": "\u684c\u9762\u6642\u9418\u63d2\u4ef6。",
  "version": "1.0.0",
  "author": {
    "name": "Example Author"
  },
  "icon": "clock",
  "type": "qml",
  "entry": "Main.qml",
  "enabledByDefault": false
}
```

Supported metadata fields are `id`, localized `name`, localized `description`, `version`, `author`, `icon`, `type`, `entry`, and `enabledByDefault`. `type` must be `qml` or `effect`. The entry must stay inside the plugin directory; canonical-path validation rejects path escapes.

QML plugins are discovered from the LunaDash shell `plugins/<id>/` directory, including the installed `$prefix/share/lunadash/shell/plugins/<id>/` path. Their root entry should be an `Item` with a `required property var shell`; it may create Quickshell windows or other shell objects. Enabled QML plugins are picked up by the shell without loading native code.

The built-in example is `qml/plugins/digital-clock/`. It displays a theme-coloured desktop clock with large hour/minute text plus month, date, and weekday. Its visual structure is inspired by the Caelestia desktop clock while using only LunaDash/Qt/Quickshell APIs.

## Native C++ effects

The new schema may also use `"type": "effect"` with a shared library filename in `entry`. Native effects execute inside the compositor, are not sandboxed, and require a session restart when their enabled state changes.

For backward compatibility, LunaDash also accepts the older KDE-inspired metadata format below. **It does not implement KWin's ABI and cannot load KWin plugins.**

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

Native plugins are discovered from `~/.local/share/ludash/plugins/<id>/`, `$prefix/share/ludash/plugins/<id>/`, and `plugins/` next to the executable. Derive the C++ class from `QObject` and `LunaDash::CompositorPlugin`. Implement `windowOpened(QQuickItem*)` and `windowFocused(QQuickItem*)`, embed matching metadata using `Q_PLUGIN_METADATA`, and declare the interface with `Q_INTERFACES`. Use `QPointer<QQuickItem>` if retaining a window reference.

The native example is in `src/compositor/plugins/fade/FadePlugin.hpp`, `src/compositor/plugins/fade/FadePlugin.cpp`, and `data/plugins/fade/metadata.json`. It builds into `build/plugins/org.ludash.fade/`.

The loader validates JSON size, ID, API/type, canonical entry paths, and the Qt plugin IID for native effects. There is currently no signing or sandbox for C++ effects, so enabling one explicitly trusts its native code.

Metadata discovery lives in `src/config/plugins/PluginCatalog`, independently of the compositor loader. The C++ namespace is now `LunaDash`; rebuild native plugins against the current header. The retained effect interface uses Qt Quick items and is not yet connected to wlroots scene-window callbacks. Loading a native module does not establish that its visual effects are active in the wlroots session.
