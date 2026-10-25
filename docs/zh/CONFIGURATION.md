# 首次啟動與設定

[English](../en/CONFIGURATION.md) · [繁中索引](README.md)

第一次登入的 Quickshell 導覽提供語言與減少動態效果控制，並連結 LunaDash 網站。獨立的 `lunadash-desktop --app welcome` 使用另一個 Slint Wayland 視窗，並透過相同 control socket 操作。按 **開始使用桌面** 會記錄初始設定完成，即使離線也可完成；只開啟文件不會完成設定。一般設定仍可重新開啟導覽；網路偏好留在 Settings。

## 網路

LunaDash 每 30 秒透過 `nmcli` 讀取 NetworkManager 裝置、已儲存連線與 Wi‑Fi 資訊，並在 LunaDash 網路操作後立即更新。若沒有 `nmcli`，改讀 NetworkManager D-Bus 狀態；若沒有 NetworkManager，則只回報可能存在的網路 link。LunaDash 不會自行建立帳號、啟用服務或探測外部連線；只有使用者在 Settings 明確要求時才修改 connection profile。連線狀態不會直接當成已驗證 Internet：captive portal 與未知連線狀態會分開顯示。

**設定網路** 會優先開 `nm-connection-editor`，否則嘗試在 Kitty／Konsole／Alacritty／foot 中執行 `nmtui`。密碼留在外部工具，不寫入 LunaDash IPC。

原生 Wi‑Fi credential 表單與 captive portal 瀏覽流程目前未實作。

## 儲存的偏好

桌面偏好、桌布狀態與快捷鍵會以原子寫入方式儲存至 `${XDG_CONFIG_HOME:-$HOME/.config}/LunaDash/desktop-preferences.json`，檔案權限僅限擁有者讀寫。首次使用各設定類別時，LunaDash 會從舊版 `LunaDash.conf`／`LunaDash.ini` 匯入有效的 `desktop/*` 值、初始設定完成狀態、桌布選擇／shader／歷史／收藏／revision，以及快捷鍵，並保留舊檔。語言等其他外觀設定仍留在舊檔，等待後續遷移。

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

## 除錯

`lunadashctl debug` 會顯示 `status` 沒有的資訊：各輸出的畫格時間、輸入裝置、鍵盤與指標焦點（由哪個視窗、
layer surface 或 popup 持有）、游標圖示與主題、layer surface、輔助程序、event loop 時間與使用中的協定。

```sh
lunadashctl debug              # 摘要
lunadashctl debug cursor       # 單一區段：outputs、input、cursor、layers、windows、
                               # capture、processes、eventLoop、protocols
lunadashctl debug json         # 完整 JSON
lunadashctl debug watch 2      # 每 2 秒更新摘要
lunadashctl debug log 200      # 最新 session log 的最後 200 行
lunadashctl debug bundle /tmp  # debug.json、status.json、session.log 與過濾後的環境變數
```

`environment.txt` 只包含顯示與工作階段相關變數（`WAYLAND_DISPLAY`、`XDG_*`、`LANG`、renderer 與游標設定）。Session log 可能含有視窗標題與路徑，分享前請先檢查。

更深入的介面自訂可修改 `LunaDash-Shell/`，但那是可信任的本機程式碼，不是 sandbox theme。Plugin 另見 [Plugin SDK 2](PLUGINS.md)，完整設定中心見 [Settings](SETTINGS.md)。
