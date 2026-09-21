# 視窗操作（dev）

[English](../en/WINDOWS.md) · [繁中索引](README.md)

預設使用 persistent split tree，把普通視窗限制在單一 screen 的 work area。兩個視窗左右分割；第三個切右側 tile 的上下；之後持續切最大 tile 的較長邊。關閉 tile 時 sibling 接回空間。Group 最多 8 個成員並垂直排列。

Taskbar 依 workspace 將已 map 的視窗包成 capsule，和 tiling column 無直接關係。預設 10 workspace、6 px gap。舊 default column-width 已退休。

| 操作 | 結果 |
| --- | --- |
| 開新普通視窗 | 切割最大 tile；第一個視窗用 requested size，必要時縮進 work area |
| 點 task | 切 workspace 並最大化 selected tiled window；再次點 active task 還原 |
| Alt + 左拖（單 active tile） | 在 work area 內移動 |
| Alt + 左拖到另一窗中央 | 交換 slot，slot 尺寸不變 |
| Alt + 左拖到 top/bottom edge | 插入 group 前/後，最多 8 member |
| Shift + Alt + 左拖 | 調 shared split boundary / row height |
| Alt+Tab | 2×5 workspace overview |
| 放開 Alt / 點 cell | 切換 workspace |
| Esc | 取消 overview |
| Super+T / Super+Return | 開預設 terminal |

普通 window 維持 tiled；parent dialog 可疊在 parent 上。Minimized window 仍占 group capacity。Maximize 只改 presentation：其他 tile 暫時隱藏，但原 split ratio、row height、順序保留；restore 完整回復。

## 2×5 Overview

Overview 狀態最多每 16 ms coalesce。Backdrop 用一個非同步 grim snapshot，最多等 350 ms，失敗改用 wallpaper。每個 window texture 先縮到最多 320×200 再 readback/worker encode。

十個 cell 固定存在，包括空 workspace。Tab/Shift+Tab、左右、scroll 每次一格；上下跨 5 格。選到超過目前 workspace count 的 cell 會在需要時擴張。`Super+0` 對應 workspace 10。Thumbnail 是 opening-time snapshot，不是 live video。

## Layout strategy

Drag/drop 完成後先解除 interactive override，再由 shared scene transition 動畫到最後矩形；interactive resize 即時。

預設 tiling。啟用 Plugin SDK 2 的 stacking `window-layout` replacement 後，可切成獨立重疊 rectangles；停用時即時回 tiling。見 [視窗配置模板](WINDOW_LAYOUT_TEMPLATES.md) 與 [stacking example](../../examples/plugins/stacking-windows/README.md)。

## 驗證

CTest 的 window-layout 測試覆蓋最多 64 window 的 bounded geometry、split insertion、output resize、8-member capacity、minimize/restore、swap、group insertion、resize distribution、workspace transfer、maximize/restore 與 shortcut migration。

真實 session 仍要驗 Alt drag、modifier release、快速 Alt+Tab、popup/dialog、client size hint、display scaling 與多輸出。CI 不代表實際 frame rate。
