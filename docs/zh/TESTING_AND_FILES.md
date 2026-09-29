# 建置、測試與原始碼地圖

[English](../en/TESTING_AND_FILES.md) · [繁中索引](README.md)

Arch Linux 是主要開發環境。完成程式、packaging 與文件修改後再建置；CI 設定本身不等於某個 commit 的實際 workflow 已成功。

## 建置與靜態檢查

需要 CMake 3.21+、Ninja、Python 3、C11/C++20、Qt 6.4+（Core/Gui/Widgets/Quick/OpenGL/Concurrent/Network/DBus）、wlroots 0.17–0.20、Wayland scanner/protocols、xkbcommon、pixman、libdrm、GL headers 與 GLib。Shell 另外需要 Quickshell 0.3+。XWayland、grim/slurp、brightnessctl、ddcutil 分別提供相容層、區域截圖與亮度控制。

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
| `qml` | Shell UI |
| `data` | assets/translations/metadata |
| `scripts`, `tests` | install/diagnostics/regression |
| `site` | 公開網站 |

架構原則見 [原始碼架構](ARCHITECTURE.md)。
