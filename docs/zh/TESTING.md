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
