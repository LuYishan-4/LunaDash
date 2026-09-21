# Desktop Plugin SDK 2

[English](../en/PLUGINS.md) · [Target 參考](PLUGIN_TARGETS.md) · [繁中索引](README.md)

LunaDash Plugin SDK 2 提供 feature hook、visual slot 與共用 metadata。內建 tiling desktop 是預設；plugin 可 replace feature 或 augment 既有功能。所有 native effect 預設關閉。

## 類型

| `type` | 實作 | Runtime |
| --- | --- | --- |
| `effect` | C11 / C++20 | 版本化 C ABI，同步 JSON hook |
| `quickshell` | QML / JavaScript | 指定 Shell visual slot |
| `opengl` | GLSL `.vert/.frag` | SDK 建置的 Qt Quick shader package |

Registry 是 `data/plugins/targets.json`。

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
    "text": {"type": "string", "default": "Hello"}
  }
}
```

`replace` 同 target 只能一個 replacement；`augment` 在 built-in 後依 plugin ID 疊加。Replacement 失敗會保留 built-in。要求 `windowTemplate: stacking` 的 window-layout plugin 必須是 replacement。現在執行 `lunadash-create-plugin --type effect --target window-layout ...` 會從通用 native effect template 建立；實際的 stacking / cascade 實作改成 `data/plugins/stacking-windows` 下的正式 SDK 2 plugin package，compositor core 只保留 stacking-mode 互動所需的通用 freeform geometry state。

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

搜尋順序先 user data，再 system data，並保留舊 path 相容。Plugin store 尚未實作；metadata/QML/native 都以使用者權限執行，protocol/session/system-service ownership 仍屬 host。
