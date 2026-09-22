# Plugin target 參考

[English](../en/PLUGIN_TARGETS.md) · [Plugin SDK 2](PLUGINS.md)

Machine-readable registry：`data/plugins/targets.json`。

```sh
lunadash-create-plugin --list
```

模板安裝於 `share/lunadash/plugin-sdk/templates`。這是開發 API，不保證跨 SDK major 的 native binary ABI。

## Visual targets

共同 context 有 `target`、`source`、`builtinSettings`、`mode`、`reportError(message)`。

| Target | 功能 |
| --- | --- |
| `panel` | taskbar/workspace/tray |
| `taskbar-windows` | window list；`groups` |
| `wallpaper` | wallpaper/context menu |
| `wallpaper-transition` | imageSource/previous/incoming/progress |
| `launcher` | launcher |
| `overview` | dashboard |
| `media` | media controller/player selection |
| `calendar` | calendar popup |
| `desktop-menu` | desktop menu |
| `audio/network/devices` | popups |
| `session` | session controls |
| `image-picker` | image chooser |
| `settings` | settings content；recovery pages 保留 built-in |
| `setup` | welcome |
| `compatibility` | compatibility launcher |
| `feedback/notifications/screenshot` | feedback |
| `workspace-transition/startup` | animations |
| `window-decoration` | border overlay |
| `window-switcher` | Alt+Tab overview |
| `tiling-hint` | drop hint |
| `blur` | switcher backdrop |
| `settings.*` | individual settings pages |

`desktop-widgets` 只接受 Quickshell，能自行建立 PanelWindows。其他 feature 應使用 host slot 以保留 input/lifecycle/fallback。

## Native hook targets

| Target | Response | 約束 |
| --- | --- | --- |
| `window-animation` | duration/offset/opacity/scale/easing | 完整 profile；桌面 animation off 仍優先 |
| `shell-animation` | duration | 0–600 ms |
| `window-rules` | workspace/maximized | initial map，workspace 必須有效 |
| `window-layout` | windows[] rectangles | ID 必須一一對應、正尺寸、在 work area |

一般 layout 禁止 overlap。Native replacement 宣告 `windowTemplate: stacking` 可選 host 的 persistent stacking strategy；hook context 會提供 `windowTemplate` 與 `allowOverlap`。Host 仍持有 membership、focus、minimize/maximize 與 workspace state。

Stacking 時 Alt+left-drag 移動，Shift+Alt+left-drag resize；focus-to-front。範例：[stacking-windows](../../examples/plugins/stacking-windows/README.md)。

## 新增 target

1. 更新 `targets.json`。
2. 在真實 feature boundary 放 `ExtensionSlot` 或呼叫 native filter。
3. 定義並驗證 context/response。
4. Replacement ready 前一定保留 built-in。
5. 補文件與 regression test。

Registry entry 本身不會自動實作 host hook；沒有實際 mount/call 的 target 不應發布。
