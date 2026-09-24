# 參照 NyxNiri 的桌面整合 — 1.0.1a

[English](../en/NYXNIRI_DESKTOP.md) · [繁中索引](README.md)

本次參照 [ech678/NyxNiri](https://github.com/ech678/NyxNiri) 的 `adf0954c877d2c70de12a51b9fd6727a59352f98`。上游組合了 Niri、Noctalia 和應用程式設定；LunaDash 保留原有 C++20／wlroots compositor、C11 底層輔助、Quickshell、模組登錄與插件介面，不另啟 Niri、Hyprland 或第二套 shell。既有使用者設定與自訂快捷鍵優先保留。

## 桌面與操作

新設定採用有邊距的膠囊面板、編號工作區、置中時鐘與媒體膠囊。右側精簡控制中心提供控制、媒體、聲音及系統頁；設定 > 儀表板可切回大型儀表板。設定視窗仍共用原本的標頭與搜尋。Dock 固定項目來自 `dock` 模組設定，執行中的視窗沿用原有 task 啟用行為；目前工作區有可見應用程式時，自動隱藏會保留 8 px 的喚出區域。

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

## Orbit 設定

設定 > 外觀提供 JSON 編輯器，檔案位於 `$XDG_CONFIG_HOME/lunadash/orbit.json`，預設為 `~/.config/lunadash/orbit.json`。隨附模板是 `data/launcher/orbit.json`，可設定程式、巢狀資料夾、網頁連結及搜尋／AI 網站。Tab／Shift+Tab 切換搜尋引擎、Alt+1–8 啟用項目、Esc 返回上一層或關閉。查詢經 URL 編碼後交給瀏覽器，不是內建 AI 用戶端。

文件需要 `schemaVersion: 1`、`defaultEngine`、`searchEngines` 和 `items`；搜尋引擎 1–8 個、每層 1–8 個項目，資料夾最多三層。每個項目有同層唯一的 `id`、`name`，及 `action`、`desktopId`、`command`、`url`、`children` 其中一種。指令必須使用參數陣列；檔案路徑與搜尋文字不交給 shell 求值。網頁限 HTTP(S)。格式錯誤或超過大小限制的修改不會破壞已儲存設定，也可還原隨附模板。

## 桌布與外觀

桌布庫掃描設定的絕對路徑及下一層分類資料夾，最多 512 個檔案，合併內建與最近使用項目。搜尋、靜態／動態篩選、分類、隨機切換及本機手動路徑共用既有選擇器。圖片限制為 64 MiB／3200 萬像素；影片限制 2 GiB，接受 MP4、WebM、MKV、MOV、M4V，實際解碼取決於安裝的 codec。影片靜音循環並位於所有視窗後方。FFmpeg 為選取的影片建立快取封面，不會一次解碼整個資料庫；缺少 Multimedia 支援時回報錯誤並保留可取得的封面。

桌布色彩會從縮小圖片或影片封面取得主色，再產生 LunaDash 色盤；不是 Noctalia 完全相同的 Material HCT 演算法。QML 與原生對話框共用淺色、深色及自動模式，自動模式在本地 07:00–19:00 使用淺色。關閉桌布取色後可手動設定重點色。

護眼模式讓 shell 表面不透明，並套用 2500–6500 K 輸出色溫。wlroots 0.20 使用 scene color transform；較舊支援版本在 backend 可用時使用 gamma ramp。不支援的輸出會回報錯誤，不冒充已生效。一般 client 模糊仍受既有 compositor 限制，這次沒有新增完整 blur pass。外觀預設集儲存在 `lunadash/presets`，只保存經驗證的外觀設定，不包含應用程式資料、外部檔案或影片本身。

Settings portal 提供配色、對比與減少動態效果提示。選擇啟用應用程式佈景同步後，會產生 LunaDash 管理的 GTK 3／4 CSS 與 Kitty 色彩檔、加入 include、設定 GTK 深色偏好，並在可用時要求 GNOME 配色提示。Kitty 在重新載入／啟動後讀取色彩，瀏覽器是否跟隨由其 portal 支援決定。Fcitx 同步需另外啟用，會將隨附 Mellow 模板套色並透過 classicui 選用。修改已存在的 CSS、Kitty 或 Fcitx 設定前會保留第一次的 `.lunadash-backup`。關閉同步只停止更新，不自動還原外部檔案；撤回整合時可移除 LunaDash include 或還原備份，再選回先前的 GTK／Fcitx 主題。個人設定建議維持獨立 include。

Mellow 模板改編自 NyxMellow，GPL-3.0 授權及來源標示位於 `data/themes/nyxmellow/`。其餘 UI 使用 LunaDash 共用元件，沒有重新散布上游桌布包。

## 相依套件與驗證界線

Arch 套件新增動態桌布需要的 `qt6-multimedia` 與 `ffmpeg`，其餘沿用 Quickshell、Qt Wayland、wlroots、Fcitx、PulseAudio／PipeWire 等相依。只有選取影片時才載入 Qt Multimedia，所以 native 編譯不需要 Multimedia 開發標頭。其他發行版需安裝 Qt 6 Multimedia 的 **QML 模組**、可用的多媒體 backend／codec 及 FFmpeg，例如 Debian／Ubuntu 常見的 `qml6-module-qtmultimedia`、Fedora 的 `qt6-qtmultimedia`。靜態桌布不需影片 codec；天氣沿用既有 Qt Network 相依。可攜 CMake 及相容程式分支不代表已驗證所有發行版或 GPU。

這仍是開發中的整合，並未達成 NyxNiri 全功能一致。Niri 專屬的分頁 column、跨輸出導覽與 glow 預設；Noctalia 的 greeter／安全鎖屏協定；上游 Python 安裝器、整套應用程式預設及部署回復系統；Fish／Starship／Zed／fastfetch 設定，以及下載式桌布包，尚未移植。本專案仍使用既有的平鋪／堆疊模板、更新器、工作階段控制與外部程式角色。Native 插件預設停用，也沒有沙箱隔離。

依本次要求，本機只進行編譯與來源／QML／翻譯靜態稽核，不啟動 LunaDash session、不執行 runtime 整合測試，也不產生真實工作階段截圖。既有 CI 設定保持不變；其結果須與本機編譯及視覺驗證分開記錄。
