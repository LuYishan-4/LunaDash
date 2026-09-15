# Shell modules

LunaDash's Quickshell shell has nine built-in blocks: `panel`, `wallpaper`, `launcher`, `overview`, `settings`, `setup`, `session`, `feedback`, and `compatibility`. Settings > Shell modules contains a JSON editor, validation, and recovery controls for their layout and appearance. Settings, setup, and feedback cannot be disabled.

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
        "edge": "bottom",
        "x": 0,
        "y": 0
      },
    }
  }
}
```

Omitted modules and fields receive defaults; saving a partial document resets omitted settings to those defaults. `0` width/height uses the built-in size. Width is 320–3840 logical pixels (settings minimum 800); panel height is 24–96; settings/setup height is 480–2160; other explicit heights are 80–2160. All dimensions clamp to the screen. `x` and `y` are optional logical positions for movable module surfaces; zero keeps the built-in anchor. Margin and radius accept 0–64, font size 10–28. Colors accept `inherit`, `#RRGGBB`, or Qt's `#AARRGGBB` notation. Only panel supports bottom anchoring. Its height and margins form the reported `panelExtent` and layer-shell exclusive area; the compositor's `workArea` excludes that extent so windows do not cover the panel. For a top panel, settings also begins below `Theme.barHeight`. Disabling panel releases the reserved window space; use Super+D or `lunadashctl open-settings modules` to recover.

The background/foreground/accent style applies to a built-in block's main surface and direct text; internal shared controls retain the global theme. Font size applies to panel segments; individual built-in headings retain their typographic hierarchy. Wallpaper keeps its image; its background color is visible behind a transparent/missing image. Rounded image clipping and independent styles for every nested button are not implemented. Small custom dimensions can reduce usable content space.

## Recovery and IPC

With `LUDASH_CONTROL` pointing at this session's owner-only control socket:

```sh
lunadashctl open-settings modules
lunadashctl module-validate "$(cat ~/.config/LunaDash/shell-modules.json)"
lunadashctl module-reset
```

`module-save` accepts the same JSON string as validate. `module-reset` restores built-in module layout and appearance. Module JSON and native plugin settings are independent.
