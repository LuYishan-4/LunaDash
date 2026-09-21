# 建置、測試與原始碼地圖

[English](../en/TESTING_AND_FILES.md) · [繁中索引](README.md)

Arch Linux 是主要開發環境。完成程式、packaging 與文件修改後再建置；CI 設定本身不等於某個 commit 的實際 workflow 已成功。

## 建置與靜態檢查

需要 CMake 3.21+、Ninja、Python 3、C11/C++20、Qt 6.4+（Core/Gui/Widgets/Quick/OpenGL/Concurrent/Network/DBus）、wlroots 0.17–0.20、Wayland scanner/protocols、xkbcommon、GL headers 與 GIO。Shell 另外需要 Quickshell 0.3+。XWayland、grim/slurp、brightnessctl、ddcutil 分別提供相容層、區域截圖與亮度控制。

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
  -DLUDASH_BUILD_FILES_TESTS=ON -DLUDASH_BUILD_RENDERER_TESTS=ON
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

Source archive 必須含 `examples/` 與 `templates/`，不然 installed plugin SDK 會缺 example/template。

## CI

| 類別 | 內容 |
| --- | --- |
| Ubuntu build | C/C++、Files、architecture |
| Qt loading | Qt client modules 對 wlroots |
| Wayland | protocol、xdg lifecycle、headless、XWayland |
| OpenGL | shaders、renderer lifetime、software GL、staged install |
| Startup | CLI failure、renderer fallback |
| Website | Astro check/build/link/asset |
| Source/QML | architecture/source language/QML checks |
| Distro | Arch、Debian 13、Fedora 45、openSUSE Tumbleweed、Alpine Edge |
| PR analysis | clang-tidy、CodeQL、Qt lifetime、policy |

Void/Gentoo 有 installer path，但 CI breadth 較小。

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
| `src/desktop` | apps/files/audio/network/power/system |
| `src/service/portal` | FileChooser portal |
| `src/shell` | modules/media |
| `src/ctl` | control client |
| `qml` | Shell UI |
| `data` | assets/translations/metadata |
| `scripts`, `tests` | install/diagnostics/regression |
| `site` | 公開網站 |

架構原則見 [原始碼架構](ARCHITECTURE.md)。
