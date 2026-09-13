# LuDash 原生外掛

LuDash 的外掛格式參考 KDE 的 `KPlugin` metadata 結構，但 **不相容 KWin 的 ABI，也不能直接載入 KWin 外掛**。

目錄：`~/.local/share/ludash/plugins/<id>/`（使用者）或 `$prefix/share/ludash/plugins/<id>/`（系統）。開發版本也掃描執行檔旁的 `plugins/`。每個目錄需包含 `metadata.json` 與 `.so`。

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

C++ 類別需繼承 `QObject` 與 `LuDash::CompositorPlugin`，實作 `windowOpened(QQuickItem*)` 及 `windowFocused(QQuickItem*)`，透過 `Q_PLUGIN_METADATA` 嵌入同一份 metadata、`Q_INTERFACES` 宣告介面。不可保存已銷毀視窗的裸指標；需要保存時使用 `QPointer<QQuickItem>`。

完整範例：`include/LuDash/fade_plugin/FadePlugin.h`、`src/fade_plugin/FadePlugin.cpp`、`data/plugins/fade/metadata.json`。預設編譯至 `build/plugins/org.ludash.fade/`。開啟 Quickshell 設定的外掛管理後可啟用，重新啟動 compositor 才會載入。

Loader 檢查 JSON 大小、Id、ApiVersion、Type、程式庫 canonical path、Qt plugin IID 與嵌入的 Id。預設不載入任何原生外掛，metadata 的 EnabledByDefault 不能繞過使用者設定。沒有簽章或 sandbox；啟用原生外掛等同信任它在 compositor 程序內執行，惡意或錯誤外掛仍可導致當機。

參考：[KDE metadata 結構](https://develop.kde.org/docs/plasma/widget/setup/)。
