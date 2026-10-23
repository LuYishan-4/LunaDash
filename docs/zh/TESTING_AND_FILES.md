# 建置、測試與原始碼地圖

[English](../en/TESTING_AND_FILES.md) · [繁中索引](README.md)

Arch Linux 是主要開發環境。完成程式、packaging 與文件修改後再建置；CI 設定本身不等於某個 commit 的實際 workflow 已成功。

請使用 `git clone --branch dev ...` 取得開發分支。一般安裝指令會下載 `packaging/shell-revision` 記錄的 Shell 版本；網站 workflow 會在暫存目錄另外抓取 LunaDash-docs。不需要遞迴 clone。修改與提交方式見[repository 工作流程](COMPONENT_REPOS.md)。

## 建置與靜態檢查

需要 CMake 3.21+、Ninja、Python 3、C11/C++20、Rust/Cargo 1.92+（建置固定版本 Slint 1.18.1）、Qt 6.4+（仍有其他 native targets 使用 Core/Gui/Widgets/Quick/OpenGL/Concurrent/Network/DBus）、wlroots 0.17–0.20、Wayland scanner/protocols、xkbcommon、pixman、libdrm、GL headers 與 GLib。Welcome 使用 Slint Winit Wayland backend 與 FemtoVG，明確停用 Slint Qt 和 X11 backends。`lunadashctl` 使用 POSIX Unix socket 與 nlohmann/json；CMake 會使用已安裝的 nlohmann_json 3.11+，或抓取固定版本 v3.11.3。獨立 Shell 另外需要 Quickshell 0.3+。XWayland、grim/slurp、brightnessctl、ddcutil 分別提供相容層、區域截圖與亮度控制。

```sh
python3 scripts/check-source-layout.py
python3 tests/architecture/test_source_layout.py
python3 tests/renderer/test_render_architecture.py
python3 tests/renderer/test_shader_extensions.py
python3 tests/wayland/test_no_qtwayland.py
python3 tests/security/test_source_language.py
python3 scripts/ci/check_qml_actions.py
python3 scripts/ci/check_qml_style.py

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLUDASH_BUILD_RENDERER_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## Plugin SDK 2

```sh
python3 tests/plugins/test_sdk.py --sdk build/sdk-build
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software qmltestrunner -input tests/qml
```

測試覆蓋 metadata/receipt validation、enable/disable、settings、native revision reload、stacking example，以及 QML replacement/augmentation/fallback/recovery。

## Runtime / Wayland / renderer

```sh
LUDASH_TEST_NO_SHELL=1 LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
./scripts/test-xdg-lifecycle.sh "$PWD/build"
python3 tests/renderer/test_startup_failure.py build/lunadash-compositor
xvfb-run -a python3 tests/wayland/test_xwayland.py build
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ./build/lunadash-renderer-test --opengl
DESTDIR="$PWD/build/stage" cmake --install build --prefix /usr
```

Headless wlroots 使用 pixman 與隔離的 runtime/config。Lifecycle 覆蓋 map/unmap/role destruction/popup；XWayland 覆蓋 hidden helper、authenticated launch、reuse/cleanup；renderer 驗 relocation、diagnostics、partial GL cleanup 與 software GL shader draw。

Host Wayland：

```sh
LUDASH_TEST_HOST_WAYLAND=1 LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
```

真實 release 仍應人工確認 Chrome/Zed、pointer/physical keyboard、Fcitx preedit/candidate、wallpaper、animation、FileChooser portal 與 logout。

一般安裝包含執行檔／相容別名、session entry、Shell QML、翻譯、portal 設定、資產及選用的範例 plugin。啟用 `BUILD_SHARED_LIBS` 時，LunaDash 內部 shared library 會安裝至 `CMAKE_INSTALL_LIBDIR`，執行檔使用相對 runtime path；static build 不需另外安裝這些 library。內建 GLSL 已嵌入，不需額外安裝 shader 目錄。

## Arch source package

```sh
./scripts/make-source.sh
(cd packaging/arch && makepkg --cleanbuild --force --nosign)
```

Source archive 必須含 `data/plugins/` 與 `templates/`，不然 installed plugin SDK 會缺 example/template。

## CI

[Dev CI 與 Main CI](CI.md) 共用維護中的測試集。開發流程依完整 diff 選擇測試類別，相關變更建置 Arch；main 與完整流程涵蓋五個發行版、clang-tidy 與 CodeQL。Ubuntu 只建置一次，供 CTest、portal frontend、plugin SDK、Wayland／XWayland 與 software OpenGL 使用；QML、網站及 NixOS 分別回報。

[NixOS](NIXOS.md) 提供鎖定的 flake、套件與系統模組。CI 建置 x86_64、求值 aarch64 並檢查安裝後的 headless runtime；真實登入及 GPU 行為仍需實機驗證。請查看指定 commit 的完成結果。Void／Gentoo 保留 installer path，但沒有相同 CI 覆蓋。

## 原始碼地圖

| 路徑 | 職責 |
| --- | --- |
| `src/compositor/session` | startup / child environment / launch policy |
| `src/compositor/wayland` | runtime/output/surface lifetime |
| `src/compositor/input` | keyboard/pointer/IME |
| `src/compositor/layout` | WindowLayout contract/factory |
| `src/compositor/tiling` | 預設 bounded tiling |
| `src/compositor/stacking` | optional stacking |
| `src/compositor/window` | rules/frame/switcher |
| `src/compositor/animation` | scene transitions |
| `src/compositor/plugins` | Plugin SDK native hooks |
| `src/compositor/renderer` | renderer orchestration |
| `src/compositor/renderer/opengl` | GL resources/shaders |
| `src/config` | preference/localization/plugin metadata |
| `src/core` | contracts/listeners/plugin C API |
| `src/desktop` | apps/default-apps/audio/network/power/system |
| `src/service/portal` | FileChooser portal |
| `src/shell` | modules/media |
| `src/ctl` | control client |
| `nix`、`flake.nix`、`flake.lock` | Nix 套件、NixOS 模組與鎖定的相依來源 |
| LunaDash-Shell | Shell UI，透過 `packaging/shell-revision` 固定版本並獨立安裝 |
| `data` | assets/translations/metadata |
| `scripts`, `tests` | install/diagnostics/regression |
| LunaDash-docs | 公開網站，由各自的 CI 驗證並由根 Pages workflow 抓取部署 |
| `src/compositor` | compositor 原始碼，透過根目錄 CMake targets 建置 |

架構原則見 [原始碼架構](ARCHITECTURE.md)。
