# Plugin target 參考

[English](../en/PLUGIN_TARGETS.md) · [Plugin SDK 2](PLUGINS.md)

Machine-readable registry：`data/plugins/targets.json`。

```sh
lunadash-create-plugin --list
```

模板安裝於 `share/lunadash/plugin-sdk/templates`。這是開發 API，不保證跨 SDK major 的 native binary ABI。

## Visual targets

每個 registry target 也有 `selection`：`single` 表示同時間最多只能啟用一個 plugin implementation，`multiple` 才能多個共存。目前 `desktop-widgets` 是 multiple；工作列、taskbar windows、window rules、window animation、window layout 與其他單一 feature surface 都是 single-owner。啟用衝突插件時會先要求確認，確認後停用舊 target；即使手動修改設定，runtime 也不會同時啟用第二個 owner。

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

Stacking 時 Alt+left-drag 移動，Shift+Alt+left-drag resize；focus-to-front。LunaDash 已不再預裝 runtime example plugin；實際發佈 package 由外部 LunaDash-Plugins registry 驗證，主 repo 只保留通用 SDK templates。

## 新增 target

1. 更新 `targets.json`。
2. 在真實 feature boundary 放 `ExtensionSlot` 或呼叫 native filter。
3. 定義並驗證 context/response。
4. Replacement ready 前一定保留 built-in。
5. 補文件與 regression test。

Registry entry 本身不會自動實作 host hook；沒有實際 mount/call 的 target 不應發布。
