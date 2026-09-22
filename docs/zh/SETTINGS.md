# Settings 設定中心 — 1.0.1a

[English](../en/SETTINGS.md) · [繁中索引](README.md)

可以從桌布右鍵、面板入口、launcher 的 Settings，或 `lunadash-desktop --app settings` 開啟同一個 Quickshell/QML 設定介面。搜尋會索引翻譯後的分類、設定名稱、描述與關鍵字；選取結果會直接開對應 page。設定頁支援捲動，換頁會重設 scroll 位置。

Plugin 與 Shell module 都在同一個 Settings surface 中；Plugins 頁負責 enable/disable、replace/augment mode、分類 options 與 advanced JSON。Plugins 與 Shell modules 兩個 recovery 頁面本身不能被 plugin replacement 拿掉。

## 目前設定範圍

| 頁面 | LunaDash 直接控制 | 外部整合／限制 |
| --- | --- | --- |
| General | 語言、Shell font、12/24 小時、welcome、偏好重設 | 外部 app theme 不跟著改 |
| Appearance | 桌布、accent、gap、panel 高度、dashboard、blur、opacity、動畫 | 桌布 picker 在 Shell 內 |
| Windows and workspaces | 1–10 workspace、bounded tiles、最多 8-member group | 一般視窗預設 tiled |
| Keyboard shortcuts | launch/focus/group/resize/workspace/capture 等 | 重複或非法 binding 會拒絕 |
| Plugins | native / Quickshell / OpenGL extension | SDK 2、hot reload、原生預設關閉 |
| Shell modules | JSON layout、尺寸、位置、顏色 | custom QML 是可信任程式碼 |
| Display | backlight、DDC/CI、primary mode、100–300% scale | 尚無完整 multi-monitor/HDR/night-light |
| Keyboard and pointer | layout、repeat、cursor size、測試欄位 | 完整 libinput tuning 未提供 |
| Sound | 輸出/麥克風音量與 mute | 經 WirePlumber |
| Network | 連線狀態 | 設定交給 NetworkManager 工具 |
| Bluetooth | 工具狀態與套件提示 | pairing 交給 Blueman |
| Power | 電池與 power profile | 進階 policy 交給 host |
| Applications and startup | 預設 terminal/files、built-in startup、plugin/X11 launcher | arbitrary session restore 未實作 |
| Privacy/accessibility | host identity、reduced motion、native-plugin access | 無 secure lock screen / screen reader |
| System | users/time/printer/storage 工具 discovery | 授權由外部工具處理 |
| About | 版本、OS/kernel/arch、graphics、GitHub、update | install/rollback 是系統動作 |

System command 來自固定 allowlist，不經 shell evaluation。Logout 結束 LunaDash；suspend/reboot/poweroff 只走 logind D-Bus，而且 destructive action 會先確認。

## 額外偏好

```text
workspaceCount    10       (1–10)
keyboardLayout    us       (us/gb/de/fr/es/jp/tw)
keyRepeatRate     25       (0–60)
keyRepeatDelay    600 ms   (200–1500)
cursorSize        24 px    (16–64，下一次 session)
fontFamily        sans-serif
clock24Hour       true
startupApps       []
```

## IPC

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl open-settings appearance
./build/lunadashctl appearance '{"workspaceCount":6,"gap":8}'
./build/lunadashctl appearance '{"keyboardLayout":"us","keyRepeatRate":25,"keyRepeatDelay":600}'
./build/lunadashctl shortcuts '{"focusLeft":"Meta+U"}'
./build/lunadashctl reset-shortcuts
./build/lunadashctl check-update
./build/lunadashctl send-key copy
./build/lunadashctl screenshot
./build/lunadashctl capture /tmp/lunadash.png
```

`send-key` 只接受 `copy/paste/cut/selectAll` 並要求 focused client。區域截圖另見 [螢幕截圖](SCREEN_CAPTURE.md)。

## 視窗、taskbar 與 workspace

Taskbar 以 workspace capsule 分組已 map 視窗，和 tiling column 是兩套概念。點 task 時，必要時先切 workspace；一般 tiled task 會最大化該視窗並暫時隱藏同 workspace 其他 tile。再次點 active task 或按 `Super+F` 會回復原 split ratios/row heights。

新視窗切割最大 tile；Alt+drag 可換 slot／插入 group；Shift+Alt+drag 調 shared boundary；`Super+Shift+H/L` 合併到鄰近 column，`Super+Shift+E` 分離。

`Alt+Tab` 開固定 2×5 workspace overview（1–10）。Tab/Shift+Tab、方向鍵、scroll 都能移動；放開 Alt 或 click 切換，Esc 取消。`Super+0` 對應 workspace 10。Thumbnail 先縮到最多 320×200，再 readback/encode，不是 live video。

## Update 與授權

About 將目前 check 與上次 install 記錄分開。背景更新的 package confirmation 自動接受，但管理員授權不可跳過；82% 會顯示 Waiting for authorization，成功後才進 system install。沒有 graphical polkit agent 時明確失敗。

更新完成後需登出/登入或 reboot 才會完整載入新 QML。開發者可用 `LUNADASH_QML_WATCH=1`，但開始 About update/rollback 後該 Shell process 會停用 watcher。

## 桌布轉場

新圖片 ready 前保留舊圖；完成後從右下角以圓形 reveal 覆蓋。GPU path 使用 masked image，software path 使用 Canvas clip。快速選擇以最後一張為準，失敗則保留舊圖；停用 animations 時立即套用。

相關：[視窗](WINDOWS.md)、[Display](DISPLAY_AND_STARTUP.md)、[Plugin SDK 2](PLUGINS.md)、[Shell modules](MODULES.md)、[Media](MEDIA.md)。


## 1.0.1a 介面規則

Settings 可在預設大小與最大化間切換；搜尋框在頂部 header，左側分類與右側 page content 都使用與 Dashboard 相同的 glass/strong surface。Dashboard 使用固定 12 欄 bounded layout，hero、快捷操作、CPU/GPU/RAM/Storage、開啟中的視窗與桌面狀態都必須限制在各自 card 中，不能再以未限制的絕對文字座標重疊。

舊 Command Console 已從內建 app、startup 設定與測試移除。互動式命令請使用設定的 terminal。舊「Motion」不再是獨立設定項；外觀頁統一稱為 **Visual effects**，無障礙頁仍保留 **Reduced motion**。
