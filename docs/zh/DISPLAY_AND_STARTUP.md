# 顯示器控制與啟動流程

[English](../en/DISPLAY_AND_STARTUP.md) · [繁中索引](README.md)

## 亮度

Settings → Display 透過 `brightnessctl` 的 machine-readable 百分比讀取 `backlight` class，不會誤把鍵盤 LED 當成螢幕。調整是非同步的，連續拖拉會合併請求。失敗時保留控制項並顯示權限錯誤；LunaDash 不會用 root 執行桌面，也不會自行放寬 device node 權限。

```sh
lunadashctl brightness 60
```

狀態包含 `brightness.available/device/percent/busy/error`。

### 外接螢幕 DDC/CI

安裝 `ddcutil` 並在螢幕 OSD 開啟 DDC/CI。LunaDash 讀 VCP `0x10`，每個顯示器依自己的最大值換算百分比。偵測、讀取與 verified write 都有 timeout，對同一螢幕的連續請求會合併。

```sh
lunadashctl ddc-refresh
lunadashctl status
lunadashctl ddc-brightness '{"id":"<status 裡的完整 id>","percent":60}'
```

若抓不到裝置，先以工作階段使用者執行 `ddcutil detect --brief`，檢查 `i2c-dev` 與發行版的 udev 規則。LunaDash 不會自行 sudo、載入 kernel module 或改 `/dev/i2c-*` 權限。

## 解析度、更新率與 scale

Settings → Display 讀 primary wlroots output 的 mode，scale 支援 100%–300%。新 mode 會先 test/commit，必須在 **15 秒內**按 Keep changes，否則自動還原；也可手動 Revert。

```sh
lunadashctl display-configure '{"scale":1.25}'
lunadashctl display-confirm
lunadashctl display-configure '{"mode":"1920x1080@144000"}'
lunadashctl display-revert
```

未設定過的實體 output 會在 preferred resolution 中選最高 advertised refresh。巢狀視窗的實體模式由 host desktop 決定。現在仍是 primary-output 設定，不是完整多螢幕排列工具。

## 啟動與動畫

Compositor 在 Shell map 前使用深色 fallback。D-Bus/systemd activation environment 發布為非同步，不讓慢服務卡住 Wayland event loop；Shell 啟動時直接取得初始 wallpaper。

Shell 使用本機 Qt theme，避免 portal 啟動形成循環。FileChooser backend 也避免在 activation 過程向自己的 frontend 查詢；其他支援的 portal 介面交給 GTK backend。建議同時安裝 `xdg-desktop-portal` 與 `xdg-desktop-portal-gtk`。

Shell 會顯示 LunaDash logo/loading，從第一個 rendered frame 起至少 650 ms 後再淡入桌面；reduced motion 會停用 pulse/fade。隱藏面板延遲載入，buffer-only commit 不再反覆重排 client 或搶走 region selector 的鍵盤焦點。桌布 decode 尺寸跟 output/scale，沒有轉場時不維持 transition mask。

安裝後優先載入安裝路徑旁的 QML，不依賴 source checkout。`lunadash-compositor --version` 與 session log 可確認實際 revision。

`--profile` 可輸出 event-loop stall。CI 能驗證啟動與 protocol 行為，但實際 FPS、實體 refresh rate、DDC/背光仍要在目標硬體測試。
