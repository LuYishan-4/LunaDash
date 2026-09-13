# 桌面外觀

預設介面參考使用者提供的桌面圖：28px 貼頂分段狀態列、青綠／灰黑配色、半透明資訊卡、細視窗邊框與滿版街景桌布。桌面 UI 使用 Quickshell；平鋪與視窗合成仍由 C++ Wayland compositor 處理。

- 左側菱形開啟啟動器，工作區點切換 1–4；`~` 開啟主控台。
- 中央開啟檔案／筆記、顯示時鐘與 CPU 歷史；點 LuDash 切換左下資訊卡。
- 右側顯示真實 CPU／記憶體；有電池時才顯示容量。齒輪開啟設定，電源按鈕提供登出選項。
- 啟動器的「已開啟的視窗」可跨工作區聚焦、還原最小化視窗；不再使用底部 Dock。
- 左下卡片顯示本機 OS、kernel、CPU、RAM 與家目錄所在磁碟資料，不是寫死的裝飾數字。
- 預設不自動開啟 welcome 視窗。`--demo` 才開啟展示程式。

桌布設定可選擇隨附的 AI 生成花店街景、自訂 PNG／JPEG／WebP 或 OpenGL shader 桌布。自訂圖檔只接受可讀取的本機圖片，限制 64 MiB／3200 萬像素；不會下載網址。來源失效時回退至內建桌布。圖片以等比例填滿顯示，長寬比不同時會裁切。

資訊卡採 alpha 透明度，沒有宣稱支援 compositor 背景模糊。頂部的程式按鈕是實際啟動入口；尚未加入完整 system tray、音量與網路管理服務。

```sh
# 自動擷取預設桌面，不開 demo 視窗
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
# 圖片：build/desktop-preview.png；狀態：build/desktop-state.json
```

QML 色彩／字型集中於 `qml/style/Theme.qml`。C++ 系統狀態與桌布設定分別位於 `system_status`／`wallpaper` 模組；繁中仍由 `data/translations/zh_TW.json` 提供。
