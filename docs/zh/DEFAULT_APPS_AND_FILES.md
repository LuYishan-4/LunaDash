# 預設應用程式、Terminal、Files 與圖片選擇

[English](../en/DEFAULT_APPS_AND_FILES.md) · [繁中索引](README.md)

Settings → Applications and startup 可以選預設 terminal 與 file manager。Selector 會列出 launcher 能找到的 installed desktop applications，並把 role default 放最前面：terminal 預設 Kitty，files 預設 LunaDash Files。

設定值以 argument array 保存，例如 `["kitty"]` 或 `["dolphin"]`；空 array `[]` 表示使用 role default。儲存前會驗證 executable/arguments，launch 時保留 argument boundary，**不經 shell operator evaluation**。

- `Super+Return` 與 Terminal 按鈕使用選定 terminal。
- `Super+E` 與 Files 按鈕使用選定 file manager。
- `lunadash-desktop --app files` 也尊重設定。
- `--builtin` 強制打開 LunaDash Files 作 recovery。
- `--path /absolute/folder` 對 built-in 直接導航；自訂 file manager 則把 path 當單一 argument。

這些 role 只影響 LunaDash launcher，不等於 system-wide MIME default。

```sh
lunadashctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'
lunadashctl launch-default terminal
lunadashctl default-apps '{"terminal":[],"files":[]}'
```

## Wallpaper picker

`qml/imagepicker/ImagePicker.qml` 畫在 Settings surface 內，因此不會變成另一個被壓在 overlay 後方的 window。它透過 `Qt.labs.folderlistmodel` 列目錄與 PNG/JPEG/WebP，提供 Home/Pictures/parent、list/grid 與最多 512×512 preview。

超過 64 MiB 的檔案不可選；真正套用前 compositor 還會驗可讀性與 32-megapixel 上限。確認後透過 `lunadashctl wallpaper-image` 套用，取消則完全不改設定。

## LunaDash Files

原生 Qt Files 有 back/forward/up、address bar、filter、details/icon view、hidden files、排序與 file operations。現有目的地不會被覆寫。Copy/move/trash 在 UI thread 外執行，遇錯即停，沒有整批 rollback。

完整 Files 現在支援更多檔案關聯與操作，請以 [FILES.md](FILES.md) 為準。Wallpaper picker 是刻意限制成 image-only 的 Shell UI，不取代 Files 或 MIME chooser。


## Portal File Picker — 1.0.1a

xdg-desktop-portal FileChooser 改用單一 LunaDash `FilePickerDialog`，包含無系統邊框的深色標題列、拖曳縮放與最大化／還原。檔案模型、選取狀態由同一視窗持有；不再將 stack 上的 `QFileDialog` 掛到較早銷毀的外框，回傳結果會在視窗銷毀前複製，連續開啟選檔也遵守相同生命週期。

瀏覽器提供常用資料夾與已掛載媒體捷徑、上一頁／下一頁／上一層、資料夾內搜尋、隱藏檔、可排序清單、圖示檢視及圖片預覽。預覽限制 64 MiB／3200 萬像素；無法預覽的檔案仍可正常選取。Location 支援絕對路徑、相對路徑、`~/` 與本機 `file://` URL；按 Enter 只導覽或準備選取，不直接送出。Ctrl+L 聚焦 Location，Ctrl+F 聚焦搜尋；路徑一律透過 Qt 驗證，不經 shell。

Open 支援單選、多選檔案與資料夾；Save 驗證目標並於覆寫前確認。檔案類型選單支援 portal glob／MIME 篩選，應用程式傳入的額外選項也會顯示並回傳。SaveFiles 僅接受檔名，遇到重名會產生不衝突名稱；無效路徑會留在視窗中顯示原因。

Main Qt workflow 檢查連續選檔、多選、篩選、手動路徑、儲存與無效路徑，並保留選擇器截圖。這是自動化涵蓋範圍；實際桌面工作階段的外觀仍需人工確認。

已刪除的 Command Console 不再是內建應用程式；互動式命令請使用設定的 terminal。
