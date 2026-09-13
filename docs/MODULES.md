# Shell modules

LuDash's Quickshell shell has nine independently replaceable blocks: `panel`, `wallpaper`, `launcher`, `overview`, `settings`, `setup`, `session`, `feedback`, and `compatibility`. Settings > Shell modules contains a JSON editor, validation, template creation, explicit custom-code trust, and recovery controls. Each block keeps its built-in behavior unless you replace it. Settings, setup, and feedback cannot be disabled, but trusted code can replace their content.

## JSON styles

The active file is `$XDG_CONFIG_HOME/LuDash/shell-modules.json` (normally `~/.config/LuDash/shell-modules.json`). Changes are saved atomically with owner-only file permissions. External edits are watched; invalid JSON retains the last valid configuration. The editor's Reload button discards unsaved text. Reload before saving if the file changed externally. The full normalized document is at most 16 KiB. Unknown keys, types, module names, and unsupported versions are rejected before writing.

```json
{
  "schemaVersion": 1,
  "modules": {
    "panel": {
      "enabled": true,
      "style": {
        "width": 1200,
        "height": 48,
        "margin": 10,
        "radius": 24,
        "background": "#ee18202b",
        "foreground": "inherit",
        "accent": "inherit",
        "fontSize": 13,
        "edge": "bottom"
      },
      "custom": { "enabled": false, "entry": "panel/Main.qml" }
    }
  }
}
```

Omitted modules and fields receive defaults; saving a partial document resets omitted settings to those defaults. `0` width/height uses the built-in size. Width is 320–3840 logical pixels (settings minimum 800); panel height is 24–96; settings/setup height is 480–2160; other explicit heights are 80–2160. All dimensions clamp to the screen. Margin and radius accept 0–64, font size 10–28. Colors accept `inherit`, `#RRGGBB`, or Qt's `#AARRGGBB` notation. Only panel supports bottom anchoring. Its height and margins reserve space in the compositor's tiling area. Disabling panel releases that space; use Super+D or `ludashctl open-settings modules` to recover.

The background/foreground/accent style applies to a built-in block's main surface and direct text; internal shared controls retain the global theme. Font size applies to panel segments and is provided to custom components; individual built-in headings retain their typographic hierarchy. Wallpaper keeps its image; its background color is visible behind a transparent/missing image. Rounded image clipping and independent styles for every nested button are not implemented. Small custom dimensions can reduce usable content space.

## Write a component

Create a panel or dashboard template in Settings. It writes `modules/panel/Main.qml` or `modules/overview/Main.qml` beside the JSON file. Existing code is never overwritten. Installed examples also live under `/usr/share/ludash/modules/templates/`. Edit the file in your preferred editor, set its relative `custom.entry`, set `custom.enabled` to true, then explicitly allow custom code.

```qml
import QtQuick
Item {
    id: root
    required property var shell
    required property var style
    required property string moduleId
    Rectangle {
        anchors.fill: parent
        radius: root.style.radius
        color: root.style.background
    }
    Text {
        anchors.centerIn: parent
        text: "My workspace"
        color: root.style.accent
        font.pixelSize: root.style.fontSize
    }
}
```

Contract version 1:

- Root must be a `QtQuick.Item`, with the three required properties above. The host controls its parent, size, Wayland layer, visibility, and open/close animation. Do not create another `Window`, `PanelWindow`, or `ShellRoot` for this block.
- `style` contains resolved colors, sizes and spacing. Use anchors/layouts to adapt to the host size. Do not assign to the host's properties or destroy it.
- `shell.state` is a read-only-by-convention snapshot updated periodically. Use `shell.command(method, value)`, `shell.setAppearance(object)`, `shell.launch("files" | "terminal")`, `shell.tr(text)`, or shell UI flags such as `settingsOpen`. Do not mutate the snapshot to represent a saved setting.
- Entrypoints must match `module/Main.qml` using ASCII letters, digits, `_` and `-`, resolve inside the module directory, be readable regular files, and be at most 256 KiB. Entry file changes trigger reload. Changes to imported helper files require touching the entrypoint; this is not a recursive dependency watcher.
- Keep callbacks short, handle missing snapshot fields, and use asynchronous work. Provide close/settings controls if replacing a full overlay. Keep visible strings in English or a separate translation pack.

**Custom QML is trusted executable code, not a sandbox.** Imports can read files, start processes and use the network with your user permissions. Path validation only constrains the entrypoint, not its imports or runtime actions. Review all imports and dependencies before enabling it. Syntax/load failures fall back to built-in content and appear in Settings and the Quickshell log. Runtime exceptions, infinite loops, memory exhaustion, and deliberate process termination cannot be contained in the same shell process. Never describe custom QML as isolated or safe simply because metadata validates. Native C++ plugins remain a separate, disabled-by-default facility.

## Recovery and IPC

With `LUDASH_CONTROL` pointing at this session's owner-only control socket:

```sh
ludashctl open-settings modules
ludashctl module-validate "$(cat ~/.config/LuDash/shell-modules.json)"
ludashctl module-code-trust false
ludashctl module-reset
```

`module-save` accepts the same JSON string as validate. `module-template panel` creates a starter file; `module-template overview` creates the other. `module-reset` restores built-ins and disables custom code, preserving user QML files. IPC remains available if custom shell content is broken, provided the compositor is running. If the shell is hung, disable custom code through IPC and restart the LuDash session. Offline, remove `modules/allowCustomCode=true` from LuDash's settings file and rename the module JSON before restarting. Module JSON and native plugin settings are independent.
