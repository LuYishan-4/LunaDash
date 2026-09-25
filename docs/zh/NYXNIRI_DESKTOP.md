# 參照 NyxNiri 的桌面整合 — 1.0.1a

[English](../en/NYXNIRI_DESKTOP.md) · [繁中索引](README.md)

本次參照 [ech678/NyxNiri](https://github.com/ech678/NyxNiri) 的 `adf0954c877d2c70de12a51b9fd6727a59352f98`。上游組合了 Niri、Noctalia 和應用程式設定；LunaDash 保留原有 C++20／wlroots compositor、C11 底層輔助、Quickshell、模組登錄與插件介面，不另啟 Niri、Hyprland 或第二套 shell。既有使用者設定與自訂快捷鍵優先保留。

## 桌面與操作

新設定參照提供的截圖：頂部保留邊距，左側依序顯示編號工作區、執行中應用程式、目前視窗標題及 CPU／記憶體資訊；啟動器置中，狀態控制與時鐘放在右側。工作區清單顯示已有視窗的工作區、目前工作區及一個備用空白工作區；只改變面板顯示方式，所有已設定的工作區與快捷鍵仍可使用。右側精簡控制中心提供控制、媒體、聲音及系統頁；設定 > 儀表板可切回大型儀表板。設定視窗仍共用原本的標頭與搜尋。

底部快捷 Dock 與桌布大型時鐘預設關閉，讓桌布如參考圖保持清爽。可在設定 > 外觀 > 顯示 Dock 手動啟用 Dock，或在桌面小工具擴充設定中啟用桌布時鐘；既有使用者儲存的偏好會保留。啟用後，固定項目來自 `dock` 模組設定，執行中的視窗沿用原有 task 啟用行為；目前工作區有可見應用程式時，自動隱藏會保留 8 px 的喚出區域。

預設視窗排列把第一個視窗保留在右半部，後續視窗分割目前焦點格子並交替分割方向。沿用既有 bounded tiling 模板建立參考配置；後續分割優先保留每格至少 320 × 300 邏輯像素，空間不足時改分割較大的窗格，因此原本的大窗之後也可能分割。設定 > 視窗與工作區可調整分割目標及初始方向，詳見[視窗配置模板](WINDOW_LAYOUT_TEMPLATES.md)。

`orbit` 和 `dock` 使用原有模組 schema 與 extension slot。`desktop-widgets` 仍是桌布 Background layer 裡可複選的插件目標，內建設定可調整時鐘位置及選擇播放音訊環形頻譜。頻譜使用既有的輸出監聽 helper，不擷取麥克風。天氣預設關閉；啟用後，會在啟動、位置變更及每 15 分鐘，把手動輸入且四捨五入至小數三位的座標傳給 [Open-Meteo](https://open-meteo.com/en/docs)。請求失敗會顯示資料無法取得，不會自動定位。

| 預設快捷鍵 | 操作 |
| --- | --- |
| Super+A／Super+滑鼠前進鍵 | Orbit |
| Super+W | 桌布選擇器 |
| Super+Ctrl+W | 隨機桌布 |
| Super+N | 護眼模式 |
| Super+grave | 常駐終端機 scratchpad |
| Super+I | 控制中心 |
| Super+V | 剪貼簿歷史 |
| Super+X | 工作階段控制 |
| Super+Shift+T | 浮動／平鋪視窗 |
| Super+Shift+F | 全螢幕 |

Super+T 和 Super+Return 保留啟動預設終端機的行為；新增快捷鍵不覆蓋既有自訂指派。Scratchpad 追蹤啟動的終端機程序及其子程序，隱藏時不關閉，顯示時跟隨目前工作區。若終端機會把啟動要求轉交給既有伺服器，需在終端機角色參數中指定建立獨立程序。啟動失敗或 15 秒內未取得視窗的錯誤，會出現在 control status 的 `scratchpad.error`。浮動視窗支援 Alt+拖曳及 Alt+Shift+拖曳調整大小；client 自行要求移動／縮放仍須有效的輸入序號。

## 可編輯的桌面設定

設定 > 外觀 > 面板配置現在直接提供所在邊緣、全長／自訂長度、厚度、邊距、內容顯示、膠囊不透明度及染色比例。所有變更經由原有具版本檢查的模組設定後端儲存並即時套用。寬度代表沿所選邊緣的長度，直立面板亦同；高度代表厚度。面板厚度與兩側邊距會為應用程式保留空間。短面板保留必要控制，程式及工作區清單可捲動，空間足夠時才顯示長標題及系統資訊。膠囊預設採用低比例次要色的深色背景，選取的工作區保留重點色。Shell 模組仍提供進階欄位。

啟動器改用原本的 LunaDash 圖標。「選擇啟動器圖片」開啟既有本機圖片選擇器，「還原 LunaDash 圖標」移除自訂圖片。支援 PNG、JPEG、WebP、GIF 及 SVG，圖片遺失或無法讀取時自動顯示預設圖標。圖片使用選取的本機路徑，請保留該檔案。關閉置中啟動器會將其移到左側；短面板也會移至左側，避免控制項重疊。

對應檔案是 `$XDG_CONFIG_HOME/LuDash/shell-modules.json`，一般為 `~/.config/LuDash/shell-modules.json`。在原有文件的 `modules.panel.config` 物件中編輯：

```json
{
  "centerLauncher": true,
  "showSystemStats": true,
  "showActiveTitle": true,
  "occupiedWorkspacesOnly": true,
  "capsuleTint": 12,
  "launcherImage": ""
}
```

這是設定片段，不能直接取代整份檔案。儲存文件時，省略欄位會採用登錄中的預設值，因此請保留其他模組設定。系統會監看外部編輯；不合法的值不會取代上一份有效設定。`data/modules/templates/shell-modules.json` 提供預設頂部面板的精簡範例。包括 Dock 顯示在內的其他外觀控制仍由桌面偏好管理；程式角色、快捷鍵、Orbit 與插件維持各自原有的設定來源。使用者可以透過 LunaDash 支援的 JSON 及設定介面建立可調整的作者風格配置，不解析 Niri KDL，也不匯入其他 compositor runtime。

首次安裝引導、可選常用軟體、方便工具及作者應用程式設定匯入，請參閱[工作階段安裝](LOGIN_SESSION.md)。安裝程式保留已有的使用者檔案；匯入的應用程式設定仍可在各自程式中修改。

## Orbit 設定

設定 > 外觀提供 JSON 編輯器，檔案位於 `$XDG_CONFIG_HOME/lunadash/orbit.json`，預設為 `~/.config/lunadash/orbit.json`。隨附模板是 `data/launcher/orbit.json`，可設定程式、巢狀資料夾、網頁連結及搜尋／AI 網站。Tab／Shift+Tab 切換搜尋引擎、Alt+1–8 啟用項目、Esc 返回上一層或關閉。查詢經 URL 編碼後交給瀏覽器，不是內建 AI 用戶端。

文件需要 `schemaVersion: 1`、`defaultEngine`、`searchEngines` 和 `items`；搜尋引擎 1–8 個、每層 1–8 個項目，資料夾最多三層。每個項目有同層唯一的 `id`、`name`，及 `action`、`desktopId`、`command`、`url`、`children` 其中一種。指令必須使用參數陣列；檔案路徑與搜尋文字不交給 shell 求值。網頁限 HTTP(S)。格式錯誤或超過大小限制的修改不會破壞已儲存設定，也可還原隨附模板。

## 桌布與外觀

桌布庫掃描設定的絕對路徑及下一層分類資料夾，最多 512 個檔案，合併內建與最近使用項目。搜尋、靜態／動態篩選、分類、隨機切換及本機手動路徑共用既有選擇器。圖片限制為 64 MiB／3200 萬像素；影片限制 2 GiB，接受 MP4、WebM、MKV、MOV、M4V，實際解碼取決於安裝的 codec。影片靜音循環並位於所有視窗後方。FFmpeg 為選取的影片建立快取封面，不會一次解碼整個資料庫；缺少 Multimedia 支援時回報錯誤並保留可取得的封面。

桌布色彩會從縮小圖片或影片封面取得主色，再產生 LunaDash 色盤；不是 Noctalia 完全相同的 Material HCT 演算法。QML 與原生對話框共用淺色、深色及自動模式，自動模式在本地 07:00–19:00 使用淺色。關閉桌布取色後可手動設定重點色。

護眼模式讓 shell 表面不透明，並套用 2500–6500 K 輸出色溫。wlroots 0.20 使用 scene color transform；較舊支援版本在 backend 可用時使用 gamma ramp。不支援的輸出會回報錯誤，不冒充已生效。一般 Wayland 與 XWayland 視窗現在共用 compositor 背景模糊路徑；不透明度控制與驗證界線請參閱[視覺效果](EFFECTS.md)。外觀預設集儲存在 `lunadash/presets`，只保存經驗證的外觀設定，不包含應用程式資料、外部檔案或影片本身。

Settings portal 提供配色、對比與減少動態效果提示。選擇啟用應用程式佈景同步後，會產生 LunaDash 管理的 GTK 3／4 CSS 與 Kitty 色彩檔、加入 include、設定 GTK 深色偏好，並在可用時要求 GNOME 配色提示。Kitty 在重新載入／啟動後讀取色彩，瀏覽器是否跟隨由其 portal 支援決定。Fcitx 同步需另外啟用，會將隨附 Mellow 模板套色並透過 classicui 選用。修改已存在的 CSS、Kitty 或 Fcitx 設定前會保留第一次的 `.lunadash-backup`。關閉同步只停止更新，不自動還原外部檔案；撤回整合時可移除 LunaDash include 或還原備份，再選回先前的 GTK／Fcitx 主題。個人設定建議維持獨立 include。

Mellow 模板改編自 NyxMellow，GPL-3.0 授權及來源標示位於 `data/themes/nyxmellow/`。其餘 UI 使用 LunaDash 共用元件，沒有重新散布上游桌布包。

## 相依套件與驗證界線

Arch 套件新增動態桌布需要的 `qt6-multimedia` 與 `ffmpeg`，其餘沿用 Quickshell、Qt Wayland、wlroots、Fcitx、PulseAudio／PipeWire 等相依。只有選取影片時才載入 Qt Multimedia，所以 native 編譯不需要 Multimedia 開發標頭。其他發行版需安裝 Qt 6 Multimedia 的 **QML 模組**、可用的多媒體 backend／codec 及 FFmpeg，例如 Debian／Ubuntu 常見的 `qml6-module-qtmultimedia`、Fedora 的 `qt6-qtmultimedia`。靜態桌布不需影片 codec；天氣沿用既有 Qt Network 相依。可攜 CMake 及相容程式分支不代表已驗證所有發行版或 GPU。

這仍是開發中的整合，並未達成 NyxNiri 全功能一致。Niri 專屬的分頁 column、跨輸出導覽與 glow 預設；Noctalia 的 greeter／安全鎖屏協定；上游整套應用程式預設及部署回復系統，以及下載式桌布包，尚未移植。可選的作者應用程式設定匯入由 LunaDash 安裝程式處理，與 compositor 分開。本專案仍使用既有的平鋪／堆疊模板、更新器、工作階段控制與外部程式角色。Native 插件預設停用，也沒有沙箱隔離。

依本次布局及安裝改動要求，不在本機執行測試或建置，也不執行來源／QML／翻譯稽核。驗證由 push 後的 CI 工作流程執行；有 CI 設定不代表已通過。Arch shell 工作流程使用軟體繪圖器，擷取執行中的 Quickshell 桌面及一般 Wayland 視窗。實體 GPU 效能與真實登入工作階段截圖仍不在此驗證範圍。
