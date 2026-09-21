# X11 / XWayland 相容

[English](../en/XWAYLAND.md) · [繁中索引](README.md)

LunaDash 仍是 Wayland compositor。XWayland 是可選、按需啟動的 rootful 相容容器；其 root window 像一般 LunaDash window 一樣進 tiling/focus/effect，但容器內 X11 app 不會被 LunaDash 個別平鋪，因為 LunaDash 沒有另外實作 X11 WM。

Arch：

```sh
sudo pacman -S --needed xorg-xwayland
```

## 純 X11 app

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl launch-x11 'xterm'
./build/lunadashctl status
```

這條路徑設定 `DISPLAY`、`XAUTHORITY` 與 X11 toolkit backend，並移除該 child 的 `WAYLAND_DISPLAY`。Argument 不經 shell evaluation。

## Native Wayland + X11 helper

某些 app 主 UI 用 Wayland，但 input/clipboard helper 需要 X11：

```sh
lunadashctl launch-with-x11 -- application --argument
```

它會先準備 authenticated XWayland、保留 Wayland 環境、讓新 server 的 root window 保持隱藏、重用既有 display/authority，並逐一保留 `--` 後的 arguments。缺少 XWayland 時直接回錯誤。

Discord 1.0.157 的 input helper 是目前已知案例。Launcher 會在 Discord 啟動前準備 server；直接從舊 terminal 執行 `flatpak run` 可能繞過流程。

## 隔離與生命週期

LunaDash 保留本機 X socket、限制 owner、把 listening FD 傳給 XWayland，用系統亂數建立 owner-only MIT-MAGIC-COOKIE-1 authority，並停用 TCP。不使用 `-ac`，也不覆寫 host authority。

這是 authentication，不是 sandbox；共享同一 X server 的 client 仍遵循 X11 trust model。

Shutdown 只終止自己擁有的 process group，並清除 authority/socket。Status 提供 `xwayland.available/running/mode/rootWindowVisible/display/authority/error`，但不輸出 cookie。啟動前設 `LUDASH_DISABLE_XWAYLAND=1` 可停用。

```sh
xvfb-run -a python3 tests/wayland/test_xwayland.py build
```

## 限制

沒有 rootless satellite 與容器內 X11 WM。Clipboard、drag/drop、games、input grab、HiDPI、Wine/Java、多螢幕與 pointer constraints 仍需 app-specific 測試。關閉 shared compatibility container 會讓其中所有 X11 app 斷線。

## GPU Screen Recorder UI

若要讓其 native Wayland overlay 仍能使用 X11 helper：

```sh
lunadashctl launch-with-x11 -- flatpak run --socket=x11 --command=env \
  com.dec05eba.gpu_screen_recorder \
  "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:?Run this from a LunaDash terminal}" \
  XDG_CURRENT_DESKTOP=river gsr-ui launch-show
```

`XDG_CURRENT_DESKTOP=river` 只對該 process 生效，用來選其已支援的 layer-shell backend，不會改 LunaDash 身分。`--command=env` 在 Flatpak 完成 socket mapping 後恢復真實 Wayland socket 名稱。

Flatpak GPU Screen Recorder 6.1.2 的記錄中，GPU/capture discovery 能找到 HDMI output，native UI 也能建立 1920×1080 layer-shell OVERLAY 並顯示在 taskbar/native windows 上方；**錄影本身仍未在該記錄中驗證**。
