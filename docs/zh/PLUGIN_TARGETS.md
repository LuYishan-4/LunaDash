# Plugin target 參考

[English](../en/PLUGIN_TARGETS.md) · [Plugin SDK 2](PLUGINS.md)

Machine-readable registry：`data/plugins/targets.json`。

```sh
lunadash-create-plugin --list
```

用 `--target`、`--id`、`--output` 產生 native effect 專案（`window-animation` 可加 `--language c`），產生器直接寫出檔案，專案使用 LunaDash CMake SDK。這是開發 API，不保證跨 SDK major 的 native binary ABI。

## Native hook targets

所有 target 都是合成器原生 hook。視覺類的 Quickshell／OpenGL slot 屬於舊 QML 殼層，已隨之移除。

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
2. 在真實 feature boundary 呼叫 native filter。
3. 定義並驗證 context/response。
4. Replacement ready 前一定保留 built-in。
5. 補文件與 regression test。

Registry entry 本身不會自動實作 host hook；沒有實際 mount/call 的 target 不應發布。
