# Shell Modules

[English](../en/MODULES.md) · [繁中索引](README.md)

LunaDash Quickshell 有 9 個 built-in block：

```text
panel wallpaper launcher overview settings setup session feedback compatibility
```

Settings → Shell modules 提供所有支援 JSON field 的 visual editor，另有 Advanced JSON。Settings、setup、feedback 是 recovery module，不能停用。

## 設定檔

```text
$XDG_CONFIG_HOME/LuDash/shell-modules.json
```

一般為 `~/.config/LuDash/shell-modules.json`。原子寫入、owner-only 權限，會監看外部修改；invalid JSON 時保留上一份 valid config。完整 normalized document 最大 16 KiB。

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
      "config": {},
      "custom": {"enabled": false, "entry": ""}
    }
  }
}
```

省略 field/module 會回 default；存 partial document 等於讓省略部分重設預設。主要 bounds：

- width 320–3840 logical px（Settings UI 最小 800）
- panel height 24–96
- settings/setup height 480–2160
- 其他 explicit height 80–2160
- margin/radius 0–64
- font size 10–28
- color：`inherit`、`#RRGGBB`、`#RRGGBBAA`

尺寸仍會 clamp 到 screen。只有 panel 支援 bottom anchoring；panel height+margin 會成為 `panelExtent` / layer-shell exclusive area，因此 compositor workArea 不會蓋到 panel。停用 panel 會釋放空間，可用 `Super+D` 或 `lunadashctl open-settings modules` recovery。

Launcher module 另外控制 moon launcher button：`buttonSize` 28–64、`logoScale` 50–120%、`backgroundOpacity` 0–100%、`glow`、`orbit`。

## Visual editor 與 IPC

Visual editor 和 Advanced JSON 寫同一份 normalized config，由同一套 C++ schema 驗證。

```sh
lunadashctl open-settings modules
lunadashctl module-validate "$(cat ~/.config/LuDash/shell-modules.json)"
lunadashctl module-reset
```

`module-save` 接收相同 JSON；`module-reset` 回復 built-in layout/appearance。

## 與 Plugin SDK 2 的關係

Plugin SDK 2 使用另一份 `extensions.json`，負責 built-in feature options、plugin options、replace/augment mode。Shell modules 管的是 Shell surface layout/style；兩者分開驗證與原子寫入。

Legacy trusted custom QML 仍作 migration fallback；新 extension 應使用 SDK metadata/CMake packaging。見 [Plugin SDK 2](PLUGINS.md)、[Target reference](PLUGIN_TARGETS.md)、[stacking example](../../examples/plugins/stacking-windows/README.md)。

## NyxNiri 桌面整合、Orbit 與動態桌布

新增模組、色盤與 portal 服務、快捷鍵、相依套件及驗證界線，請參閱[桌面整合說明](NYXNIRI_DESKTOP.md)。
