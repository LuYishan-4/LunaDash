# 視窗操作（dev）

[English](../en/WINDOWS.md) · [繁中索引](README.md)

預設使用 persistent split tree，把普通視窗限制在單一 screen 的 work area。兩個視窗左右分割；第三個切右側 tile 的上下；之後持續切最大 tile 的較長邊。關閉 tile 時 sibling 接回空間。Group 最多 8 個成員並垂直排列。

Taskbar 依 workspace 將已 map 的視窗包成 capsule，和 tiling column 無直接關係。新設定預設 10 workspace、12 px gap；既有使用者已儲存的 gap 不會被強制覆寫。單一 tiled window 會使用 layout template 的一致初始尺寸，不再直接繼承 app 啟動時任意回報的初始大小。舊 default column-width 已退休。

| 操作 | 結果 |
| --- | --- |
| 開新普通視窗 | 切割最大 tile；單一 tiled window 先用置中的 template 預設大小，新增視窗後再進 split tree |
| 點 task | 切 workspace 並最大化 selected tiled window；再次點 active task 還原 |
| Alt + 左拖（單 active tile） | 在 work area 內移動 |
| Alt + 左拖到另一窗中央 | 交換 slot，slot 尺寸不變 |
| Alt + 左拖到 top/bottom edge | 插入 group 前/後，最多 8 member |
| Shift + Alt + 左拖 | 調 shared split boundary / row height |
| 點一下 Meta（不搭配其他鍵） | 立即開啟應用程式搜尋；Meta+其他鍵仍照原快捷鍵執行 |
| Alt+Tab | 2×5 workspace overview |
| 滑鼠移到 cell 上 | 立即移動 selection，不需要先點一下 |
| 放開 Alt / 點 cell | 確認並切換 workspace |
| Esc | 取消 overview |
| Super+T / Super+Return | 開預設 terminal |

普通 window 預設 tiled；Super+Shift+T 切換浮動／平鋪。浮動視窗支援 Alt+拖曳移動、Alt+Shift+拖曳縮放；parent dialog 仍可疊在 parent 上。Minimized window 仍占 group capacity。Maximize 只改 presentation：其他 tile 暫時隱藏，浮動視窗保持可見，但原 split ratio、row height、順序保留；restore 完整回復。

## 2×5 Overview

Overview 狀態最多每 16 ms coalesce。Backdrop 用一個非同步 grim snapshot，最多等 350 ms，失敗改用 wallpaper。每個 window texture 先縮到最多 320×200 再 readback/worker encode。

十個 cell 固定存在，包括空 workspace。Tab/Shift+Tab、左右、scroll 每次一格；上下跨 5 格。選到超過目前 workspace count 的 cell 會在需要時擴張。`Super+0` 對應 workspace 10。Thumbnail 是 opening-time snapshot，不是 live video。

## Layout strategy

Drag/drop 完成後先解除 interactive override，再由 shared scene transition 動畫到最後矩形；relayout snapshot 會直接填滿插值中的 rectangle，新的 client content 更早 cross-fade 進來，避免舊版最後一小段才突然換畫面與移動中的黑邊。Interactive resize 仍保持即時。

預設 tiling。啟用 Plugin SDK 2 的 stacking `window-layout` replacement 後，可切成獨立重疊 rectangles；停用時即時回 tiling。見 [視窗配置模板](WINDOW_LAYOUT_TEMPLATES.md) 與 [stacking example](../../examples/plugins/stacking-windows/README.md)。

## 驗證

CTest 的 window-layout 測試覆蓋最多 64 window 的 bounded geometry、split insertion、output resize、8-member capacity、minimize/restore、swap、group insertion、resize distribution、workspace transfer、maximize/restore 與 shortcut migration。

真實 session 仍要驗 Alt drag、modifier release、快速 Alt+Tab、popup/dialog、client size hint、display scaling 與多輸出。CI 不代表實際 frame rate。


## 工作列、Dashboard 與剪貼簿

Taskbar capsule 改成較輕的樣式，active workspace／window 有清楚但不過亮的底線與 hover 動畫。視窗圖示左鍵仍是啟用；**中鍵可直接關閉該視窗**。

Dashboard 內的分頁只要滑鼠移上去就會切換，不必再點一下；Dashboard 開啟後若滑鼠離開其表面，經過短暫 grace period 會自動收起，避免一直佔住桌面。

狀態列新增 **剪貼簿** 按鈕。LunaDash 最多保存 20 筆純文字歷史，而且只放在目前登入 session 的 `$XDG_RUNTIME_DIR/lunadash/clipboard-history.json`，權限為 owner-only，不會長期寫入家目錄。圖片等 binary MIME 不進歷史。點選任一筆會透過 `wl-copy` 重新放回剪貼簿；**清除**只清掉這份 session history。需要安裝 `wl-clipboard`。

新版面板預設顯示工作區編號，仍可設定膠囊樣式。[桌面整合說明](NYXNIRI_DESKTOP.md) 列出常駐終端機 scratchpad 與新增快捷鍵。
