# LuDash

**C++20 / OpenGL Wayland compositor + Quickshell 桌面介面**，參考 KDE 的面板、啟動器與工作區概念，採用主欄／堆疊平鋪。C++ namespace 為 `LuDash`，程式碼使用英文，繁中放在獨立語言包。

目前為 0.1 開發版，尚不是完整 KDE 替代品。已實作：Quickshell 桌布／分段狀態列／資訊卡／啟動器／設定、原生 Wayland 平鋪與 4 工作區、英文／繁中、text-input 協定註冊、pacman 介面、metadata 原生外掛與淡入範例，以及檔案／筆記／命令主控台等独立工具。

## Arch 安裝依賴與建置

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-wayland qt6-translations quickshell mesa
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
QT_QPA_PLATFORM=wayland ./build/ludash-compositor --socket ludash-test
```

[Quickshell 已提供 Arch 套件](https://archlinux.org/packages/extra/x86_64/quickshell/)。其他發行版的 C++ 後端需 Qt ≥ 6.4、CMake ≥ 3.21、C++20、Wayland 開發檔與 scanner；Quickshell ≥ 0.3 需另外依其 [官方安裝方式](https://quickshell.org/docs/v0.3.0/guide/install-setup/) 配置，可能需要較新 Qt。

Ubuntu 24.04 / Debian 的後端依賴：`build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland libqt6opengl6-dev libwayland-dev pkg-config`。
Fedora 後端依賴：`gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel wayland-devel`。

CI 設定包含 Arch／Ubuntu／Fedora 後端建置；完整 Quickshell 工作階段在 Arch job 驗證。設定檔存在不代表遠端 CI 已執行或其他平台已實測。

## 使用與測試

```sh
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
./scripts/test-wayland.sh
```

自動測試需要 Xvfb、xauth、Python。成功需同時顯示畫面、通過平鋪檢查與正常退出；輸出在 `build/wayland-preview.png`、`wayland-state.json`、`wayland.log`。

另一個終端機可連到手動工作階段：

```sh
WAYLAND_DISPLAY=ludash-test QT_QPA_PLATFORM=wayland ./build/ludash-desktop --app notes
```

Quickshell 設定可切換語言與桌布、開啟輸入法與外掛管理。原生工具的語言變更下次啟動生效。套件操作會開啟 Konsole／foot／Alacritty，保留 sudo 與 pacman 確認；沒有 pacman 的系統只停用此工具。

| 快捷鍵 | 功能 |
| --- | --- |
| Super+Enter / E / D | 主控台／檔案／啟動器 |
| Super+J / K | 切換焦點 |
| Super+H / L | 主欄比例 |
| Super+1…4 | 工作區 |
| Super+Shift+1…4 | 移動視窗至工作區 |
| Super+Space / F / M / Q | 浮動／填滿／最小化／關閉 |

外層桌面可能攔截 Super 快捷鍵，可使用 Quickshell 工作區按鈕與工作列。

## 安裝

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j4
sudo cmake --install build
```

Arch 封裝：`./scripts/make-source.sh`，再於 `packaging/arch` 執行 `makepkg -Cfs`。工作階段檔 `ludash.desktop` 使用 EGLFS/KMS，仍需真機驗證；先使用 nested 模式。

## 文件與限制

- [測試方式與訊息判讀](docs/TESTING.md)
- [架構與模組分離](docs/ARCHITECTURE.md)
- [多語言與輸入法](docs/INPUT_METHODS.md)
- [metadata 外掛開發](docs/PLUGINS.md)
- [PR 資安與 crash 檢查](docs/SECURITY_CHECKS.md)

尚未完成 XWayland、多螢幕、完整 layer-shell、鎖定、portal、網路／音量管理，以及 Fcitx5／IBus 完整端到端相容性。原生外掛沒有 sandbox，預設停用。授權為 GPL-3.0-only，見 LICENSE。

圖形 API 可用 `--graphics opengl|gles|auto` 選擇。自訂 context／GLSL shader 與 OpenGL ES 3.0 支援詳見 [docs/GRAPHICS.md](docs/GRAPHICS.md)。

外觀與自訂桌布：[docs/APPEARANCE.md](docs/APPEARANCE.md)。
