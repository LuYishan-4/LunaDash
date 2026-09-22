# X11 / XWayland 相容

[English](../en/XWAYLAND.md) · [繁中索引](README.md)

LunaDash 仍然是 Wayland compositor。XWayland 現在由 wlroots 以 **lazy + rootless XWM** 模式管理，不再自行啟動 rootful Xwayland 容器。

Arch：

```sh
sudo pacman -S --needed xorg-xwayland
```

啟動 compositor 前設定 `LUDASH_DISABLE_XWAYLAND=1` 可完全停用 X11 相容層。

## X11 視窗

LunaDash 建立一個 `wlr_xwayland`，並把它綁到既有的 `wlr_seat`。wlroots 會先保留 `DISPLAY`；真正有 X11 client 連線時才啟動 Xwayland。

每個 `wlr_xwayland_surface` 都會獨立變成 LunaDash 的 `ClientWindow`，不再塞進一個 rootful 大視窗。因此 X11 視窗可以各自使用：

- 工作區與工作列；
- focus / Alt+Tab；
- tiling / freeform layout；
- minimize / maximize / fullscreen / close；
- 視窗規則與既有 scene graph。

生命週期依 wlroots 的規則處理：

```text
new_surface
→ associate
→ wl_surface map / unmap
→ dissociate
→ destroy
```

只有 `associate` 到 `dissociate` 之間才會使用內部的 `wlr_surface`。

## XWM selection bridge 與剪貼簿

XWayland XWM 透過 `wlr_xwayland_set_seat()` 使用 LunaDash 原本的 seat。這個 seat 同時提供 `wl_data_device_manager` 與 `zwlr_data_control_manager_v1`，所以 X11 與 Wayland 不再有兩套互不相通的 clipboard：

```text
X11 app
   ↕
wlroots XWM selection bridge
   ↕
wlr_seat
   ↕
Wayland data-device / data-control
   ↕
Wayland app + LunaDash clipboard history
```

因此 X11 視窗按 Ctrl+C 後，selection 可以進 Wayland data-control，LunaDash 的 clipboard history 也能收到。

history 不再只記純文字：

- text：直接保存；
- file / URI list：保留原 URI MIME；
- image；
- audio；
- video；
- 其他 binary MIME。

圖片、影音與其他 binary payload 會存到 owner-only 的
`$XDG_RUNTIME_DIR/lunadash/clipboard-payloads/`；JSON history 只保存 MIME、類型、大小、摘要與 payload 路徑。重新點選時用原本 MIME 交給 `wl-copy`，不會把檔案或媒體轉成亂碼文字。

CI 的 `tests/wayland/test_xwayland.py` 會直接測：

- X11 `xclip` → Wayland `wl-paste` → LunaDash history；
- Wayland `wl-copy` → X11 `xclip`。

## 啟動方式

純 X11：

```sh
lunadashctl launch-x11 'application --argument'
```

Native Wayland 主程式但需要 X11 helper：

```sh
lunadashctl launch-with-x11 -- application --argument
```

第二種會同時保留 `WAYLAND_DISPLAY` 與加入 `DISPLAY`。哪些應用需要 X11 helper 仍由 `data/session/launch-capabilities.json` 描述，不在 compositor 內硬編碼 Chromium / Electron 參數。

## Session environment

XWayland 建立後，LunaDash 會把 `DISPLAY` 一起發布到 D-Bus activation environment 與 systemd user manager，所以 portal、D-Bus activation 或 user service 啟動的 helper 也能看到同一個 display。

Status 會提供：

- `xwayland.available`
- `xwayland.running`
- `xwayland.mode = "rootless-lazy-xwm"`
- `xwayland.rootWindowVisible = false`
- `xwayland.selectionBridge`
- `xwayland.display`
- `xwayland.error`

這個架構不再由 LunaDash 建立 Xauthority 檔，也沒有 rootful X11 desktop window。

## 測試

```sh
xvfb-run -a python3 tests/wayland/test_xwayland.py build
```

測試包含 lazy startup、X11 視窗個別管理、雙向 selection bridge、clipboard history 與 clean shutdown。X11 的特殊 input grab、少見 DnD MIME、Wine/Java 與多螢幕行為仍需要真實 session 測試。
