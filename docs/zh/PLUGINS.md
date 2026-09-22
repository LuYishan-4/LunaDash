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
  "version": "1.0.1a",
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

`replace` 同 target 只能一個 replacement；`augment` 在 built-in 後依 plugin ID 疊加。Replacement 失敗會保留 built-in。要求 `windowTemplate: stacking` 的 window-layout plugin 必須是 replacement。現在執行 `lunadash-create-plugin --type effect --target window-layout ...` 會從通用 native effect template 建立；實際的 stacking / cascade 實作改成 `data/plugins/stacking-windows` 下的正式 SDK 2 plugin package，compositor core 只保留 stacking-mode 互動所需的通用 freeform geometry state。

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

Store 只改 discovery 來源，既有 SDK/CMake 安裝流程不變；真正執行的本機 package 仍必須通過 `metadata.json`、SDK receipt 與 native ABI 驗證。出現在 Store 不會自動啟用 native plugin。

Metadata/QML/native 都以使用者權限執行，protocol/session/system-service ownership 仍屬 host。


## Desktop widgets 與 Audio Wave

`desktop-widgets` 是 multi-selection target，可同時啟用 Digital Clock、Audio Wave 與其他桌布 widget。它們由 `Wallpaper.qml` 嵌入 layer-shell Background surface；desktop widget 不應自行建立 Top/Overlay window 來覆蓋一般應用程式。

Audio Wave 1.0.1a 現在透過 `lunadash-shell-tool audio-spectrum` 持續讀取 `parec` 的播放輸出 monitor（也支援 PipeWire-Pulse）。24 kHz stereo PCM 使用 1024 samples Hann FFT、512 samples hop，產生 32 個對數頻帶，左右聲道分別分析以避免反相抵消。每秒約 47 次更新；限制緩衝，不儲存或上傳音訊，也不讀取預設麥克風。需要 `parec`（Arch 的 `libpulse`、Debian/Ubuntu 的 `pulseaudio-utils`）與運作中的 PulseAudio 相容服務。

插件依畫面幀時間平滑升降（attack 22 ms / release 140 ms），使用對數強度與可調 spectrum gain，預設高度 320 px。不再輪詢 MPRIS 或產生正弦假動畫，因此瀏覽器聲音也能驅動。靜音、無聲或樣本中斷會回到底線；缺少 helper/server 時顯示原因並每三秒重試。預設輸出改變後，shell 裝置清單更新會重新連接 monitor。插件始終留在桌布 Background layer。

Store 除版本號外，也比較整包來源檔案雜湊的安裝紀錄。因此同樣標示 `1.0.1a` 的修訂仍能更新；舊安裝沒有紀錄時會提供一次更新。紀錄僅用於版本辨識，不代表沙箱或信任保證。下載仍先暫存、逐檔校驗，更新完成後仍須明確啟用。

Audio Wave 新增「Wave amplitude」振幅倍率，預設 1.8、可調 0.5–3 倍；這個新設定也套用到保留舊高度設定的安裝。自動峰值正規化提高較小聲播放的可見度，同時保留頻帶高低差；可關閉，靜音與低於 PCM 門檻的訊號不放大。柱間距與增益也可調整，基準高度 320 px，峰值動畫使用 22/140 ms 上升／下降時間。
