# Desktop Plugin SDK 2

[English](../en/PLUGINS.md) · [Target 參考](PLUGIN_TARGETS.md) · [繁中索引](README.md)

LunaDash Plugin SDK 2 提供 feature hook、visual slot 與共用 metadata。內建 tiling desktop 是預設；plugin 可 replace feature 或 augment 既有功能。所有 native effect 預設關閉。

## 類型

| `type` | 實作 | Runtime |
| --- | --- | --- |
| `effect` | C11 / C++20 | 版本化 C ABI，同步 JSON hook |
| `quickshell` | QML / JavaScript | 指定 Shell visual slot |
| `opengl` | GLSL `.vert/.frag` | SDK 建置的 Qt Quick shader package |

Registry 是 `data/plugins/targets.json`。一個 package 可以使用舊的單 target 欄位，也可以宣告 `targets` 陣列；每個 target 都有獨立的 `type / target / mode / entry / settings`，但共用同一個 package ID、名稱與版本。

## 建立與安裝

```sh
lunadash-create-plugin --list
lunadash-create-plugin --type quickshell --target panel \
  --id org.example.panel --output my-panel
cmake -S my-panel -B my-panel/build -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build my-panel/build
cmake --install my-panel/build
```

每個 project 使用：

```cmake
find_package(LunaDashPlugin 2 CONFIG REQUIRED)
lunadash_add_plugin(my_plugin
    METADATA "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json"
    SOURCES Effect.cpp)
```

QML/JS/assets 用 `FILES` 明確列出。不要只手動 copy source folder；SDK 會驗證 metadata、建立 build receipt 並安裝完整 package。

## Metadata / mode

```json
{
  "schemaVersion": 2,
  "sdk": {"name": "LunaDash", "apiVersion": 2},
  "id": "org.example.panel",
  "name": "My panel",
  "version": "1.0.0",
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

Target registry 中標記為 `selection: "single"` 的功能同時間只允許一個 plugin implementation。使用者啟用另一個衝突插件時，設定介面會先列出衝突項目；確認後自動停用舊 target、再啟用新的。Backend 也會拒絕手動 JSON 繞過限制。`desktop-widgets` 保持可多選；即使是可組合 target，`replace` 仍只能有一個。要求 `windowTemplate: stacking` 的 window-layout plugin 必須是 replacement。

Multi-target package 的 runtime config 會依 target 分開：

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

Plugin 的 settings 不需要另外寫設定 QML。Host 會透過共用 Settings API 自動產生與 LunaDash 原生介面一致的控制項；Plugin SDK 只允許四種：**是/否 toggle**、**下拉 select**、**數值輸入 number**、**數值拉條 slider**。自由文字與 array 類控制保留給 host/module 內部設定，第三方 plugin metadata 會被 SDK 與 runtime 雙重拒絕。

## Native hook 與 runtime loading

作者實作同步、stateless 的：

```c
ludash_plugin_process(request, response, capacity)
```

Request 帶 `target/mode/context/builtin/current/settings`。SDK 產生 `ludash_plugin_entry_v2`、嵌入 manifest 並建立 receipt。

不要保存 host pointer、開 background thread、註冊跨 callback、跑 nested event loop 或長時間 block。Native code 沒 sandbox，仍能讓 compositor crash/hang。

Enable/disable 與設定變更不需 logout/reboot。Plugin package 直接從安裝目錄使用，不再建立 private revision copy、每秒 fingerprint 或背景監看 package。Native library 啟用期間會保持載入，因此替換 binary 前先 disable，再完成替換後重新 enable。直接載入失敗時會 fallback built-in；「重試插件」會清除載入錯誤並重新嘗試目前安裝的 package。

## Quickshell

Entry point：

```qml
required property var shell
required property var settings
required property var context
```

Host 在 construction 前注入。可用 `shell.command`、`shell.launch`、`shell.openUrl`。`pluginReady: false` 可延後 replacement。

## OpenGL package

```json
"shaders": {"vertex": "Effect.vert", "fragment": "Effect.frag"}
```

SDK 用 `qsb` 編譯。Shader 只作用於 Quickshell visual slot；不能直接處理 arbitrary app buffer 或 wlroots output framebuffer。Software rendering 留用 built-in。

## 設定與 recovery

`~/.config/LuDash/extensions.json` 原子寫入，最大 24 KiB。

```sh
lunadashctl extension-save '<JSON>'
lunadashctl open-settings plugins
```

搜尋順序先 user data，再 system data，並保留舊 path 相容。

## 社群 Registry 與 Plugin Store

社群 plugin 統一透過 [LunaDash-Plugins](https://github.com/LuYishan-4/LunaDash-Plugins) 投稿。每個 plugin 放在 `plugins/<id>/`，PR 會依 LunaDash SDK 2 的 target、manifest 與 settings schema 驗證，必須通過 registry CI 與 maintainer review 後才會進入產生的 catalogue；同一份 reviewed registry 也由 Astro 網站提供瀏覽。

Settings → Plugins → Store 現在直接讀取 [LunaDash-Plugins](https://github.com/LuYishan-4/LunaDash-Plugins) 的 reviewed registry。Runtime 預設透過 HTTPS 取得 `https://raw.githubusercontent.com/LuYishan-4/LunaDash-Plugins/main/index.json`，在交給 QML 前驗證 catalogue 格式、plugin ID、target/type、tags 與 remote URL；網路不可用時使用內建的同版 registry fallback。開發者可用 `LUNADASH_PLUGIN_CATALOG_URL` 指向其他 HTTPS index，或設為 `off` 停用遠端 refresh。

Store 除了 discovery，也支援 reviewed 的純 QML package 一鍵下載。只有 catalogue 明確提供 `install.files` 的 package 才會出現下載按鈕；LunaDash 逐檔以 HTTPS 下載、驗證 SHA-256、先寫入使用者暫存目錄、建立 SDK receipt，再重新驗證 `metadata.json`。下載完成後保持停用，使用者只需調整自動產生的參數並自行開啟。

使用者下載的 package 位於 `~/.local/share/lunadash/plugins/<id>/`。Installed 卡片只對這個 user root 下的 plugin 顯示「刪除」；系統 plugin 不可從此處刪除。舊 schema 1 / legacy plugin 不再顯示於新版 Plugins QML，但 parser 保留 migration 相容。Native/effect package 仍走 SDK/CMake 安裝流程，Store 不會因為上架就自動啟用。

Metadata/QML/native 都以使用者權限執行，protocol/session/system-service ownership 仍屬 host。
