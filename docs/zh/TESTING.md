# LunaDash 測試快速入口

[English](../en/TESTING.md) · [完整測試文件](TESTING_AND_FILES.md)

維護中的完整 build、static、runtime、staged-install 指令都集中在 [TESTING_AND_FILES.md](TESTING_AND_FILES.md)。Graphics 測試要區分兩條不同路徑：[GRAPHICS.md](GRAPHICS.md) 說明 Qt software OpenGL render-element test 與活動 wlroots headless pixman session test 的差異。

如果已在 Wayland desktop 中：

```sh
./scripts/test-once.sh
```

它會 build 並啟動 nested session，輸出：

```text
build-once/wayland.log
build-once/wayland-state.json
```

這個 helper **目前不會自動截圖**。Release 前仍要用真實 session 檢查 Chrome/Zed、physical input、wallpaper/animation、portal、XWayland、Fcitx 與登入/登出行為。


`Arch Quickshell desktop layout` workflow 在 headless Wayland compositor 上啟動實際 shell，上傳 1920×1080 與 1280×720 桌面／控制中心 PNG、協定日誌與 JSON 狀態，檢查視窗界線、QML 執行錯誤及預設停用的底部 Dock。這些是軟體工作階段產物，不能取代實體 GPU／登入截圖。本次修改依要求禁止本地建置與測試，驗證只在 CI 執行。
同一個 job 也會開啟六個 Qt Wayland 檔案管理器視窗，檢查遞迴分割、可用空間及建議最小尺寸，切換實際模糊並上傳毛玻璃／不透明截圖。

Arch shell 工作流程透過 Qt 輔助使用動作與 Wayland 鍵盤輸入操作真正的設定控制，涵蓋面板尺寸、染色及自訂啟動器圖片／還原。六個 Qt Wayland 檔案管理員視窗會檢查可用區域、無重疊、建議最小尺寸及實際 client 幾何，切換模糊並比對圓角與桌布像素，保存設定及桌面截圖。原生 CI 測試另涵蓋圓角覆蓋及玻璃底層生命週期。這些是軟體 Wayland 工作階段驗證，實體 GPU／登入工作階段仍需另行確認。
