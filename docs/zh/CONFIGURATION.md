# 首次啟動與設定

[English](../en/CONFIGURATION.md) · [繁中索引](README.md)

第一次登入會顯示簡單的歡迎畫面與 LunaDash 網站連結。按 **開始使用桌面** 會記錄 setup complete，即使離線也可完成。語言、網路與外觀都留在 Settings 中，歡迎頁不會替你修改這些偏好。

## 網路

LunaDash 每五秒透過 NetworkManager D-Bus 讀取既有連線狀態，不自行建立帳號、不修改 connection profile、不啟用服務，也不做外部連線探測。**設定網路** 會優先開 `nm-connection-editor`，否則嘗試在 Kitty／Konsole／Alacritty／foot 中執行 `nmtui`。密碼留在外部工具，不寫入 LunaDash IPC。

若系統沒有 NetworkManager，LunaDash 最多只能回報可能存在的 link，不能把它當作已驗證 Internet 連線。原生 Wi‑Fi credential 表單與 captive portal 流程目前未實作。

## 儲存的偏好

預設設定檔：

```text
~/.config/LuDash/LuDash.conf
```

`XDG_CONFIG_HOME` 會改變其根目錄。活動中的工作階段可能覆寫手動修改，因此要直接編輯檔案時最好先停止 LunaDash。

| Key | 預設 | 範圍／值 |
| --- | --- | --- |
| `desktop/accent` | `#9ccbfb` | `#RRGGBB` |
| `desktop/gap` | 12 | 4–32 px |
| `desktop/panelHeight` | 40 | 32–56 px |
| `desktop/overview` | false | bool |
| `desktop/blur` | true | bool |
| `desktop/blurRadius` | 18 | 0–32 |
| `desktop/windowOpacity` | 96 | 60–100% |
| `desktop/animations` | true | bool |
| `desktop/animationDuration` | 220 | 0–600 ms |
| `desktop/showHostDetails` | false | bool |
| `appearance/language` | system locale | `en_US`, `zh_TW`, `zh_CN`, `ja_JP` |
| `appearance/wallpaperMode` | image | image / shader |
| `appearance/wallpaperImage` | bundled | 已驗證的本機圖片 |
| `session/setupComplete` | false | bool |

`LUDASH_LANGUAGE` 會覆蓋儲存語言；`LUDASH_SKIP_SETUP=1` 可供自動測試略過 guide，但不寫入完成狀態。

## IPC

巢狀工作階段例：

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl status
./build/lunadashctl appearance '{"accent":"#c4b5fd","gap":20,"panelHeight":32}'
./build/lunadashctl choose-wallpaper
./build/lunadashctl wallpaper-image /absolute/path/wallpaper.png
./build/lunadashctl wallpaper-default
./build/lunadashctl language zh_TW
./build/lunadashctl setup
```

未知 key、錯誤型別、超出範圍或壞掉的 JSON 會在修改任何偏好前被拒絕。桌布選擇器位於 Shell 內，支援 PNG/JPEG/WebP、目錄瀏覽與 bounded preview；單檔超過 64 MiB 不可選，compositor 還會再次驗證可讀性與像素上限。

更深入的介面自訂可修改 `qml/`，但那是可信任的本機程式碼，不是 sandbox theme。Plugin 另見 [Plugin SDK 2](PLUGINS.md)，完整設定中心見 [Settings](SETTINGS.md)。
