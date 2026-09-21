# 螢幕截圖

[English](../en/SCREEN_CAPTURE.md) · [繁中索引](README.md)

按 **Meta+Shift+S** 選矩形區域。拖曳後放開完成；Escape 取消。LunaDash 在自己的 Wayland session 啟動 `slurp`，選擇完成後把 geometry 傳給 `grim`。兩者都在 installer dependencies 中。

截圖以 private PNG 存到 Pictures/Screenshots。取消不會改上一張截圖，也不建立檔案。舊版的 Alt+Shift+F5 binding 會在不衝突時遷移到 Meta+Shift+S，使用者自訂或已停用 binding 會保留。

Selector 的 stdin 會關閉，因此能立即顯示而不是等 rectangle candidates。選擇中再按一次 shortcut 不會重啟 selector；Escape 仍只取消，不顯示假錯誤。

## IPC

```sh
lunadashctl screenshot
```

這會非同步開始 region selection，立即回 `pending/phase`。用 `lunadashctl status` 查：

```text
screenCapture.phase = selecting | capturing | saved | cancelled | failed
screenCapture.busy
screenCapture.lastCapture
screenCapture.error
```

自動化需要 full-output capture 時：

```sh
lunadashctl capture /absolute/path.png
lunadash-compositor --screenshot /absolute/path.png
```

這兩個不開 region selector，並拒絕覆寫既有檔案。

Compositor 會 advertise wlroots screencopy、xdg-output、idle-inhibit global。Release 前仍要在真實 session 測 region geometry、Escape、重複 shortcut、HiDPI、多輸出與 PNG 權限。

FileChooser portal 與 screen capture 是不同功能；完整 PipeWire screen-sharing portal、per-toplevel capture policy、screen-lock integration 目前尚未完成。
