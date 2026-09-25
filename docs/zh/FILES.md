# 檔案管理與 Dolphin

LunaDash 預設使用 **Dolphin**。安裝腳本與 Arch 套件將 `dolphin` 列為主體執行相依套件，選用 desktop 群組另外提供 Chromium、Ark 與 KIO extras。其他發行版成功建置不代表已驗證該平台的 Dolphin 整合。

Super+E、檔案捷徑及 `lunadashctl launch-default files` 都使用 files 角色。空白命令陣列會執行 `dolphin --new-window`，在目前工作區開啟新視窗。Settings > Applications and startup 可以改選其他已安裝程式，原有的明確選擇仍有效。`lunadash-desktop --app files --path /absolute/folder` 會將資料夾當作單一引數傳遞，不經 shell。

舊的原生檔案管理器、專用關聯設定介面、GIO 啟動器及複製／移動／垃圾桶引擎已從原始碼和建置目標移除，沒有內建備援管理器。瀏覽、檔案操作與程式偏好由 Dolphin 處理；檔案類型關聯請使用 Dolphin 或系統 MIME 設定。舊 LunaDash 關聯設定檔保留在原處，但不再讀取或匯入，詳見[遷移說明](FILE_ASSOCIATION_MIGRATION.md)。

## 保留的選擇器

LunaDash portal FileChooser 保留無邊框深色標題列、手動本機路徑、篩選、多選、儲存確認與有尺寸限制的預覽。圖示實作歸屬 `src/service/portal/`，不再依賴檔案管理器程式。螢幕／視窗分享選擇器與 Settings 圖片選擇器也保留。路徑驗證不經 shell，詳見[預設程式及選擇器](DEFAULT_APPS_AND_FILES.md)。

Portal 跟隨 LunaDash 配色；Dolphin 自行管理程式主題。Compositor 的視窗不透明度與毛玻璃設定如同其他一般應用程式套用，原有全螢幕及護眼模式例外保持有效。

## CI 涵蓋範圍

原生 Qt 測試涵蓋預設角色、明確自訂命令及保留的 portal 選檔行為。Arch Quickshell workflow 啟動真正的 Dolphin 視窗，檢查工作區切換、可用區域、遞迴平鋪及圓角／毛玻璃像素，並上傳軟體 Wayland 工作階段截圖。實體 GPU 與登入工作階段外觀仍需另外確認。
