# 預設應用程式、Terminal、Files 與圖片選擇

[English](../en/DEFAULT_APPS_AND_FILES.md) · [繁中索引](README.md)

Settings → Applications and startup 可以選預設 terminal 與 file manager。Selector 會列出 launcher 能找到的 installed desktop applications，並把 role default 放最前面：terminal 預設 Kitty，files 預設 Dolphin。

設定值以 argument array 保存，例如 `["kitty"]` 或 `["dolphin"]`；空 array `[]` 表示使用 role default。儲存前會驗證 executable/arguments，launch 時保留 argument boundary，**不經 shell operator evaluation**。

Native application-role service 會把新設定以 JSON 寫入 `${XDG_CONFIG_HOME:-$HOME/.config}/LunaDash/default-applications.json`。尚未寫入此檔案的 role 會從舊版 `LunaDash.conf` 或 `LunaDash.ini` 的 `defaultApps` 項目讀取；空 array 會使用該角色的 LunaDash 預設程式。

- `Super+Return` 與 Terminal 按鈕使用選定 terminal。
- `Super+E` 與 Files 按鈕使用選定 file manager。
- `lunadash-desktop --app files` 也尊重設定。
- 原本的內建檔案管理器及 `--builtin` 選項已移除。
- `--path /absolute/folder` 將路徑作為單一引數傳給預設或自訂檔案管理器。

這些 role 只影響 LunaDash launcher，不等於 system-wide MIME default。

`lunadash-desktop --app packages` 會開啟獨立的 Slint 套件管理器。已安裝套件與搜尋查詢以目前使用者執行。安裝、移除與完整升級會先顯示命令供確認，確認後才在設定的終端機開啟。終端機以引數陣列啟動，不經 shell；目前辨識 Kitty、Konsole、foot、Alacritty、xterm、GNOME Terminal、GNOME Console 與 Xfce Terminal 的執行參數形式。

```sh
lunadashctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'
lunadashctl launch-default terminal
lunadashctl default-apps '{"terminal":[],"files":[]}'
```

## Wallpaper picker

`LunaDash-Shell/imagepicker/ImagePicker.qml` 畫在 Settings surface 內，因此不會變成另一個被壓在 overlay 後方的 window。它透過 `Qt.labs.folderlistmodel` 列目錄與 PNG/JPEG/WebP，提供 Home/Pictures/parent、list/grid 與最多 512×512 preview。

超過 64 MiB 的檔案不可選；真正套用前 compositor 還會驗可讀性與 32-megapixel 上限。確認後透過 `lunadashctl wallpaper-image` 套用，取消則完全不改設定。

## Dolphin

Dolphin 列為主體執行相依套件，預設 files 角色啟動 `dolphin --new-window`。使用者已設定的外部檔案管理器命令仍然有效。內建檔案管理器、專用檔案關聯編輯器與檔案操作引擎已移除，詳見 [FILES.md](FILES.md)。Portal FileChooser、Shell 圖片選擇器及螢幕／視窗分享選擇器各自獨立，仍保留原有功能。

## Portal File Picker — 1.0.1a

xdg-desktop-portal FileChooser 會啟動獨立的 `lunadash-filepicker` Slint／Winit Wayland client。它提供有界無系統邊框視窗、導覽、常用位置、清單／格狀檢視、隱藏檔切換、搜尋、圖片預覽、手動本機路徑欄、篩選器及應用程式選項。helper 透過 stdin／stdout 傳回本機路徑與選項，由 portal backend 組成 D-Bus 回應；不會透過 shell 執行所選路徑。

瀏覽器提供常用資料夾與媒體掛載點、上一頁／下一頁／上一層、資料夾內搜尋、隱藏檔、依名稱排序的清單／格狀檢視及圖片預覽。預覽限制 64 MiB／3200 萬像素；無法預覽的檔案仍可正常選取。Location 支援絕對路徑、相對路徑、`~/` 與本機 `file://` URL；按 Enter 只導覽或準備選取，不直接送出。Ctrl+L 聚焦 Location，Ctrl+F 聚焦搜尋；helper 以 `std::filesystem` 驗證本機路徑，不經 shell。

Open 支援單選、多選檔案與資料夾；Save 驗證目標並於覆寫前確認。檔案類型選單支援 portal glob／MIME 篩選，應用程式傳入的額外選項也會顯示並回傳。SaveFiles 僅接受檔名，遇到重名會產生不衝突名稱；無效路徑會留在視窗中顯示原因。

Portal D-Bus service 使用 GLib GDBus 匯出 FileChooser v4 與 Settings v2；FileChooser 視窗與獨立 ScreenCast 來源選擇器都是 Slint／Winit Wayland client。ScreenCast 保留 xdpw 的逐行選擇協定，可在擷取可用時顯示本機螢幕／視窗預覽，並原樣回傳來源標籤。Headless Wayland 整合流程會操作 OpenFile、SaveFile、SaveFiles 與公開 portal 前端，並擷取 FileChooser 畫面。實體工作階段的視覺確認仍需另外執行。

已刪除的 Command Console 不再是內建應用程式；互動式命令請使用設定的 terminal。
