# LuDash 測試與逐檔介紹

這份文件從第一次執行測試開始，列出操作方式、預期結果、錯誤判讀與每個專案維護檔案的用途。所有命令均從專案根目錄執行。

LuDash 是 C++20／Wayland 平鋪桌面的開發版本，桌面 UI 使用 Quickshell，renderer 支援 OpenGL 3.3 Core 與 OpenGL ES 3.0。優先驗證 Arch；不是可直接取代 KDE 的完整生產環境。

## 1. 先安裝測試依賴

Arch Linux：

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-wayland \
  qt6-translations quickshell mesa xorg-server-xvfb xorg-xauth xdotool python python-pillow
```

安全檢查另需 `clang`；輸入繁中建議安裝 `noto-fonts-cjk`。Fcitx5、終端機等選用依賴見 [INPUT_METHODS.md](INPUT_METHODS.md) 與 [../packaging/arch/PKGBUILD](../packaging/arch/PKGBUILD)。

Ubuntu／Debian 後端編譯依賴：`build-essential cmake ninja-build pkg-config libwayland-dev qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland libqt6opengl6-dev`；測試加上 `libgl1-mesa-dri xvfb xauth python3 python3-pil`。Fedora 對應 `gcc-c++ cmake ninja-build wayland-devel qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel mesa-dri-drivers xorg-x11-server-Xvfb xorg-x11-xauth python3 python3-pillow`。Quickshell 0.3 以上需依該發行版另行安裝，可能需要比後端最低需求 Qt 6.4 更新的 Qt。

## 2. 完成修改後再建置

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
```

這會產生：

| 執行檔 | 用途 |
| --- | --- |
| `build/ludash-compositor` | Wayland server、平鋪、圖形 context，並啟動 Quickshell。 |
| `build/ludash-desktop` | 內建獨立應用程式入口；未指定 `--app` 時啟動 compositor。 |
| `build/ludashctl` | 透過本機 socket 查詢狀態與控制工作區、視窗、語言及桌布。 |
| `build/ludash-tests` | 原生工具與後端行為測試。 |
| `build/ludash-render-tests` | 建立 OpenGL／GLES context，編譯與連結實際 shader。 |

不要將舊執行檔的測試結果當成新程式碼已通過；改動 C++ 或資源 JSON 後需完成這一步。

## 3. 第一輪自動測試

```sh
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ctest --test-dir build --output-on-failure
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
```

`ctest` 必須是 `100% tests passed`。三個 shell 測試必須以 0 退出並輸出 `clean shutdown`。每次 shell 測試顯示約 8 秒，正常關閉最多再等 5 秒；程式碼不是以 SIGTERM 當作測試成功的清理流程。

| CTest 名稱 | 實際檢查 |
| --- | --- |
| `desktop-interactions` | 語言包、非法桌布、pacman 參數注入、外掛路徑穿越、平鋪、視窗生命週期、檔案導覽、未儲存筆記取消關閉、命令結束碼。 |
| `security-gate` | SARIF 缺失／格式錯誤／安全與品質發現是否確實造成失敗。 |
| `graphics-contexts` | 真正建立 desktop GL 與 GLES context，編譯並連結 production shader。 |
| `graphics-startup-failure` | 無效參數，以及 Mesa 3.2 無法建立要求的 3.3 context 時，應回傳 2 而非 SIGABRT。 |
| `source-language` | C++、標頭與 QML 來源不能出現中文字元；繁中只在語言包。 |

`test-wayland.sh` 除了幾何與 mapped 狀態，還檢查 shader、程序失敗、桌面圖層、本機狀態及截圖內容。展示視窗不能只有單色空白外框。桌面預覽模式則不開任何 demo 視窗，方便比對外觀。

沒有 Quickshell 的環境可使用 `LUDASH_TEST_NO_SHELL=1 ./scripts/test-wayland.sh`，只驗證 compositor 與兩個原生 Wayland 用戶端；不能把它當成桌面 UI 測試已通過。

## 4. 點擊操作與 crash 檢查

```sh
xvfb-run -a -s '-screen 0 1440x900x24' \
  python3 tests/wayland/test_shell_interactions.py build
xvfb-run -a python3 tests/wayland/test_crash_detection.py build
```

第一個測試以 xdotool 點擊本次 Xvfb 中的 LuDash 視窗，檢查工作區、啟動器、設定、語言，並透過 IPC 驗證最小化／還原與桌布切換；工作階段約 22 秒。它不操作你原本的桌面。

第二個測試只對它啟動並確認為 `ludash-desktop` 的子程序注入 SIGSEGV，預期 compositor 以 2 退出；測試腳本自身回傳 0 才表示「有成功抓到 crash」。這是刻意的負向測試，不能與一般整合測試的 crash 混為一談。

## 5. 看測試證據

| 檔案 | 看什麼 |
| --- | --- |
| `build/desktop-preview.png` | 不開 demo 視窗的預設桌面、狀態列、資訊卡與桌布。 |
| `build/desktop-state.json`、`build/desktop.log` | 預設桌面模式的狀態與日誌。 |
| `build/wayland-preview.png` | 三個 demo 程式的實際畫面，確認可見內容而非只有外框。 |
| `build/wayland-state.json` | `graphicsApi`、版本、`shaderReady`、`graphicsFailed`、`processFailure`、`layerSurfaces`、視窗座標及系統數值。 |
| `build/wayland.log` | Qt／Quickshell／compositor 輸出。 |
| `build/Testing/Temporary/LastTest.log` | CTest 的詳細輸出。 |

每輪會先清除該模式的舊截圖與 JSON；不同 graphics API 的 demo 測試會覆寫同名證據。需要同時保存 GL／GLES 證據時，測完一輪先複製成不同名稱。

## 6. 手動試用

在既有 Wayland 桌面中：

```sh
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE \
  QT_QPA_PLATFORM=wayland ./build/ludash-compositor --socket ludash-test --graphics opengl
```

如果要測 GLES，把最後一個值改成 `gles`。如果外層桌面是 X11，可使用 `QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_egl` 作為 host backend；LuDash 內部依然使用 Wayland。

另一個終端機：

```sh
WAYLAND_DISPLAY=ludash-test QT_QPA_PLATFORM=wayland ./build/ludash-desktop --app notes
WAYLAND_DISPLAY=ludash-test QT_QPA_PLATFORM=wayland ./build/ludash-desktop --app files
```

逐項操作：

1. 左上菱形開啟啟動器；開啟檔案、筆記、主控台，視窗應平鋪且避開細頂列。
2. 點工作區切換；Super+Shift+2 移動視窗，Super+J／K 切換焦點，Super+H／L 調整比例。
3. Super+M 最小化，再由啟動器「已開啟的視窗」還原；這也能跳至視窗所在工作區。
4. 點齒輪切換英文／繁中；QML 介面直接更新，獨立原生工具需重新開啟。
5. 切換花店圖片與 Dusk／Forest shader；選擇自己的圖片，長寬比應保持，重啟後保留。
6. 點中央 LuDash 標記隱藏／顯示資訊卡；CPU／RAM 數值應隨實際負載變化。
7. 筆記輸入文字後關閉，選「取消」應保留內容，選「儲存」後重開檔案應一致。
8. 主控台輸入 `printf 'hello\n'; exit 7`，應顯示 hello 與結束碼 7；此工具不是 PTY 終端機。
9. 開啟套件管理只做查詢；安裝／移除會交給真正的終端機與 pacman 確認，測試不需要變更系統套件。
10. 點電源按鈕、取消登出應留在桌面；確認登出應正常結束，未儲存筆記仍需處理。

外層 KDE／GNOME 可能先攔截 Super 組合鍵；無反應時先排除快捷鍵衝突，不要直接判定內部 Wayland 失敗。

自訂桌布的 IPC 方式：

```sh
LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control" ./build/ludashctl wallpaper-image /absolute/path/wallpaper.png
LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control" ./build/ludashctl wallpaper-default
```

## 7. ASan、UBSan 與 clang-tidy

```sh
sudo pacman -S --needed clang
cmake -S . -B build-checked -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-checked -j4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ctest --test-dir build-checked --output-on-failure
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  LUDASH_BUILD_DIR="$PWD/build-checked" ./scripts/test-wayland.sh
```

C 與 C++ 都指定 Clang，避免產生的 Wayland C 協定檔與 Qt 編譯旗標混用 GCC。若舊目錄的 compiler 不同，使用新的建置目錄；不需要刪除原始碼。

任何 sanitizer 報告、clang-tidy 警告或非零退出都算失敗。`detect_leaks=0` 代表這輪不檢查 memory leak，不能宣稱完全沒有記憶體問題。這些工具只能涵蓋實際分析／執行到的路徑。

## 8. 封裝與安裝檢查

```sh
./scripts/make-source.sh
# 原始碼封裝：packaging/arch/ludash-0.1.0.tar.gz
cmake --install build --prefix /tmp/ludash-install-check
```

暫存安裝可檢查 bin、Quickshell 設定、桌布、翻譯與範例外掛是否齊全，不會取代系統桌面。Arch 的實際套件建置於 `packaging/arch` 執行 `makepkg -Cfs`；這份本機來源 PKGBUILD 的 checksum 為 SKIP，不是已發布且經簽章驗證的遠端套件。

獨立 EGLFS/KMS 登入階段、seat／VT、多螢幕、硬體 buffer 匯入仍需真機驗證；先用 nested 模式。

## 9. 常見錯誤

| 現象 | 判讀／處理 |
| --- | --- |
| `QMessageLogger::fatal` → `QQuickWindow::event` → SIGABRT | 舊版 Qt Quick context 初始化失敗的終止路徑。新版接收 `sceneGraphError` 回傳 2；保留其具體日誌。 |
| `MESA_GL_VERSION_OVERRIDE=3.2` | 可能阻止 EGL 建立要求的 3.3 context。一般自動測試只在自身程序中移除 override；負向測試會刻意保留它。 |
| `There is no EGL_WL_bind_wayland_display extension` | Xvfb 沒有該硬體 buffer 匯入路徑。共享記憶體用戶端仍可顯示；要同時檢查 context、畫面與正常退出，不能當成真機 GPU 加速通過。 |
| `App info not found for ludash-app` | host portal 缺少開發應用程式登錄；目前不宣稱完整 portal 支援。 |
| `invalid method 8` | 舊 layer-shell 沒有 `set_layer`；重新建置含 v2 支援的 compositor。 |
| 只有外框、內容空白 | 檢查 context depth／stencil 設定與新像素驗證。Qt Quick 要有 24-bit depth／8-bit stencil 供排序與裁切使用。 |
| 找不到 Pillow／xdotool／Quickshell | 補齊第 1 節測試依賴，不要略過失敗。 |
| 一般測試出現「程序已當機」 | 新版不把這種結果當成功；保存日誌、退出碼與 stack trace。 |

Fcitx5／IBus 的完整候選字定位與跨 toolkit 相容性尚未驗證。協定已註冊不等同完整輸入法 bridge 已完成，具體測法見 [INPUT_METHODS.md](INPUT_METHODS.md)。

## 10. 本輪驗證紀錄

本輪 UI 修改後的建置與執行結果會記錄於此，區分本機實測與僅配置的 CI。

<!-- verification-results -->
待完成本輪建置與測試後填入。
<!-- /verification-results -->

## 11. 每個檔案負責什麼

以下逐項涵蓋專案維護的檔案；`build/`、`build-*/`、`.git/`、Python cache 與壓縮產物是生成資料，不逐一列入來源清單。標頭宣告介面、`.cpp` 實作功能。新增功能應保留同名模組的 include／src 目錄。

### 根目錄與 CI

| 檔案 | 用途 |
| --- | --- |
| [.clang-tidy](../.clang-tidy) | 啟用的 crash／資安相關靜態分析規則，警告視為失敗。 |
| [.github/codeql/config.yml](../.github/codeql/config.yml) | CodeQL 使用的 C/C++ 安全與品質查詢設定。 |
| [.github/dependabot.yml](../.github/dependabot.yml) | GitHub Actions 版本更新設定。 |
| [.github/pull_request_template.md](../.github/pull_request_template.md) | PR 問題、改動與驗證資訊模板。 |
| [.github/workflows/build.yml](../.github/workflows/build.yml) | Arch／Ubuntu／Fedora build 與 CTest；Arch 額外測 Quickshell。 |
| [.github/workflows/security.yml](../.github/workflows/security.yml) | PR／push 的 clang-tidy、ASan／UBSan、CodeQL 與 SARIF gate。 |
| [.gitignore](../.gitignore) | 排除 build、封裝產物、SARIF 與 Python cache。 |
| [AGENTS.md](../AGENTS.md) | 專案開發規則、命名空間、功能分層與建置時機。 |
| [CMakeLists.txt](../CMakeLists.txt) | 明列 C/C++ sources、Qt dependencies、資源、測試與安裝目標。 |
| [LICENSE](../LICENSE) | 專案 GPL-3.0-only 授權聲明。 |
| [README.md](../README.md) | 專案入口、依賴、建置、操作與已知限制摘要。 |

### C++ 標頭

| 檔案 | 用途 |
| --- | --- |
| [include/LuDash/application_catalog/ApplicationCatalog.h](../include/LuDash/application_catalog/ApplicationCatalog.h) | 內建應用程式的識別碼、顯示名稱與清單的宣告。 |
| [include/LuDash/application_window/ApplicationWindow.h](../include/LuDash/application_window/ApplicationWindow.h) | 原生工具視窗與關閉前確認回呼的宣告。 |
| [include/LuDash/compositor/ClientWindow.h](../include/LuDash/compositor/ClientWindow.h) | 每個 xdg 視窗的狀態：surface、外框、工作區、焦點與平鋪資料。 |
| [include/LuDash/compositor/WaylandCompositor.h](../include/LuDash/compositor/WaylandCompositor.h) | Wayland compositor 的公開方法、擁有資源與生命週期宣告。 |
| [include/LuDash/console/Console.h](../include/LuDash/console/Console.h) | 非互動命令主控台、子程序輸出與停止的宣告。 |
| [include/LuDash/fade_plugin/FadePlugin.h](../include/LuDash/fade_plugin/FadePlugin.h) | 隨附的視窗淡入外掛與 Qt plugin 宣告的宣告。 |
| [include/LuDash/file_manager/FileManager.h](../include/LuDash/file_manager/FileManager.h) | 檔案系統瀏覽、路徑導覽與開啟檔案的宣告。 |
| [include/LuDash/input_method/InputMethodSupport.h](../include/LuDash/input_method/InputMethodSupport.h) | 註冊 text-input v2／v3 與 Qt 輸入法協定的宣告。 |
| [include/LuDash/ipc/ControlServer.h](../include/LuDash/ipc/ControlServer.h) | 僅限本機使用者的 JSON 控制 socket、大小與逾時限制的宣告。 |
| [include/LuDash/launcher/Launcher.h](../include/LuDash/launcher/Launcher.h) | 原生工具版啟動器，供獨立應用程式與快捷鍵入口使用的宣告。 |
| [include/LuDash/layer_shell/LayerShell.h](../include/LuDash/layer_shell/LayerShell.h) | layer-shell global 的宣告與圖層 surface 清單。 |
| [include/LuDash/layer_shell/LayerSurface.h](../include/LuDash/layer_shell/LayerSurface.h) | 單一 layer surface 的狀態與協定請求處理宣告。 |
| [include/LuDash/localization/JsonTranslator.h](../include/LuDash/localization/JsonTranslator.h) | Qt QTranslator 子類與 JSON 字串字典宣告。 |
| [include/LuDash/localization/Localization.h](../include/LuDash/localization/Localization.h) | 語言選擇、字典、translate 與初始化函式宣告。 |
| [include/LuDash/notes/Notes.h](../include/LuDash/notes/Notes.h) | 文字編輯、UTF-8 讀寫、大小限制與未儲存提示的宣告。 |
| [include/LuDash/packages/PackageManager.h](../include/LuDash/packages/PackageManager.h) | pacman 查詢、參數驗證及終端機中的套件交易的宣告。 |
| [include/LuDash/plugin_settings/PluginSettings.h](../include/LuDash/plugin_settings/PluginSettings.h) | 顯示外掛 metadata 與明確啟用／停用介面的宣告。 |
| [include/LuDash/plugins/CompositorPlugin.h](../include/LuDash/plugins/CompositorPlugin.h) | 原生外掛 SDK 的介面、API 版本與 Qt IID。 |
| [include/LuDash/plugins/PluginManager.h](../include/LuDash/plugins/PluginManager.h) | metadata 資料結構、掃描／載入與事件分派宣告。 |
| [include/LuDash/renderer/RenderBackend.h](../include/LuDash/renderer/RenderBackend.h) | GraphicsApi、跨執行緒 RenderState 與 context／shader 函式宣告。 |
| [include/LuDash/renderer/WallpaperItem.h](../include/LuDash/renderer/WallpaperItem.h) | Qt Quick framebuffer 桌布 item 的宣告。 |
| [include/LuDash/renderer/WallpaperRenderer.h](../include/LuDash/renderer/WallpaperRenderer.h) | shader renderer、GL 函式、program 與 VAO 的宣告。 |
| [include/LuDash/settings/Settings.h](../include/LuDash/settings/Settings.h) | 原生設定工具：語言、輸入法、桌布與快捷鍵說明的宣告。 |
| [include/LuDash/system_monitor/SystemMonitor.h](../include/LuDash/system_monitor/SystemMonitor.h) | 獨立系統概況工具，顯示系統、RAM 與磁碟的宣告。 |
| [include/LuDash/system_status/SystemStatus.h](../include/LuDash/system_status/SystemStatus.h) | 定時讀取 /proc 與 /sys，提供 QML 狀態列及資訊卡資料的宣告。 |
| [include/LuDash/theme/DesktopTheme.h](../include/LuDash/theme/DesktopTheme.h) | Qt Widgets 工具的色彩、字型與樣式表的宣告。 |
| [include/LuDash/tiling/TilingLayout.h](../include/LuDash/tiling/TilingLayout.h) | 計算主欄／堆疊平鋪矩形，避免視窗互相重疊的宣告。 |
| [include/LuDash/wallpaper/WallpaperSettings.h](../include/LuDash/wallpaper/WallpaperSettings.h) | 本機圖片驗證、桌布偏好與內建圖片回退的宣告。 |
| [include/LuDash/welcome/Welcome.h](../include/LuDash/welcome/Welcome.h) | demo 歡迎工具與常用功能入口的宣告。 |
| [include/LuDash/window_frame/WindowFrame.h](../include/LuDash/window_frame/WindowFrame.h) | 繪製細視窗邊框／標題列並處理關閉點擊的宣告。 |

### C++ 實作與入口

| 檔案 | 用途 |
| --- | --- |
| [src/application_catalog/ApplicationCatalog.cpp](../src/application_catalog/ApplicationCatalog.cpp) | 內建應用程式的識別碼、顯示名稱與清單的實作。 |
| [src/application_window/ApplicationWindow.cpp](../src/application_window/ApplicationWindow.cpp) | 原生工具視窗與關閉前確認回呼的實作。 |
| [src/compositor/WaylandCompositor.cpp](../src/compositor/WaylandCompositor.cpp) | 建立 Wayland／輸出、管理用戶端、工作區、快捷鍵、IPC、圖形失敗與正常關閉。 |
| [src/console/Console.cpp](../src/console/Console.cpp) | 非互動命令主控台、子程序輸出與停止的實作。 |
| [src/entrypoints/compositor_main.cpp](../src/entrypoints/compositor_main.cpp) | compositor 命令列入口：圖形選項、demo、截圖、狀態與限時測試。 |
| [src/entrypoints/control_main.cpp](../src/entrypoints/control_main.cpp) | ludashctl 入口：送出 JSON 命令、讀取回應並回報退出碼。 |
| [src/entrypoints/desktop_main.cpp](../src/entrypoints/desktop_main.cpp) | 原生工具入口，依 --app 建立 files／notes／console 等程式。 |
| [src/fade_plugin/FadePlugin.cpp](../src/fade_plugin/FadePlugin.cpp) | 隨附的視窗淡入外掛與 Qt plugin 宣告的實作。 |
| [src/file_manager/FileManager.cpp](../src/file_manager/FileManager.cpp) | 檔案系統瀏覽、路徑導覽與開啟檔案的實作。 |
| [src/input_method/InputMethodSupport.cpp](../src/input_method/InputMethodSupport.cpp) | 註冊 text-input v2／v3 與 Qt 輸入法協定的實作。 |
| [src/ipc/ControlServer.cpp](../src/ipc/ControlServer.cpp) | 僅限本機使用者的 JSON 控制 socket、大小與逾時限制的實作。 |
| [src/launcher/Launcher.cpp](../src/launcher/Launcher.cpp) | 原生工具版啟動器，供獨立應用程式與快捷鍵入口使用的實作。 |
| [src/layer_shell/LayerShell.cpp](../src/layer_shell/LayerShell.cpp) | 公布／協商 layer-shell v2、建立圖層與追蹤 mapped 數。 |
| [src/layer_shell/LayerSurface.cpp](../src/layer_shell/LayerSurface.cpp) | 處理 size／anchor／margin／keyboard／set_layer、configure、ack 與資源釋放。 |
| [src/localization/Localization.cpp](../src/localization/Localization.cpp) | 載入 JSON resource、套用 Qt 翻譯與讀取語言偏好。 |
| [src/notes/Notes.cpp](../src/notes/Notes.cpp) | 文字編輯、UTF-8 讀寫、大小限制與未儲存提示的實作。 |
| [src/packages/PackageManager.cpp](../src/packages/PackageManager.cpp) | pacman 查詢、參數驗證及終端機中的套件交易的實作。 |
| [src/plugin_settings/PluginSettings.cpp](../src/plugin_settings/PluginSettings.cpp) | 顯示外掛 metadata 與明確啟用／停用介面的實作。 |
| [src/plugins/PluginManager.cpp](../src/plugins/PluginManager.cpp) | 驗證外掛路徑與 metadata，載入明確啟用的原生外掛並發送視窗事件。 |
| [src/renderer/RenderBackend.cpp](../src/renderer/RenderBackend.cpp) | 解析 GL／GLES 選項、設定 QSurfaceFormat、載入 GLSL 並加上版本前綴。 |
| [src/renderer/WallpaperItem.cpp](../src/renderer/WallpaperItem.cpp) | 連接 GUI 配色狀態與 render-thread renderer。 |
| [src/renderer/WallpaperRenderer.cpp](../src/renderer/WallpaperRenderer.cpp) | 取得 current context、驗證 API、編譯／連結 shader 並繪製 FBO。 |
| [src/settings/Settings.cpp](../src/settings/Settings.cpp) | 原生設定工具：語言、輸入法、桌布與快捷鍵說明的實作。 |
| [src/system_monitor/SystemMonitor.cpp](../src/system_monitor/SystemMonitor.cpp) | 獨立系統概況工具，顯示系統、RAM 與磁碟的實作。 |
| [src/system_status/SystemStatus.cpp](../src/system_status/SystemStatus.cpp) | 定時讀取 /proc 與 /sys，提供 QML 狀態列及資訊卡資料的實作。 |
| [src/theme/DesktopTheme.cpp](../src/theme/DesktopTheme.cpp) | Qt Widgets 工具的色彩、字型與樣式表的實作。 |
| [src/tiling/TilingLayout.cpp](../src/tiling/TilingLayout.cpp) | 計算主欄／堆疊平鋪矩形，避免視窗互相重疊的實作。 |
| [src/wallpaper/WallpaperSettings.cpp](../src/wallpaper/WallpaperSettings.cpp) | 本機圖片驗證、桌布偏好與內建圖片回退的實作。 |
| [src/welcome/Welcome.cpp](../src/welcome/Welcome.cpp) | demo 歡迎工具與常用功能入口的實作。 |
| [src/window_frame/WindowFrame.cpp](../src/window_frame/WindowFrame.cpp) | 繪製細視窗邊框／標題列並處理關閉點擊的實作。 |

### Quickshell UI

| 檔案 | 用途 |
| --- | --- |
| [qml/components/Segment.qml](../qml/components/Segment.qml) | 狀態列箭頭分段按鈕、hover 與鍵盤操作。 |
| [qml/components/ShellButton.qml](../qml/components/ShellButton.qml) | 啟動器與設定共用按鈕。 |
| [qml/feedback/Message.qml](../qml/feedback/Message.qml) | 顯示 IPC／桌布驗證錯誤，點擊可關閉。 |
| [qml/launcher/Launcher.qml](../qml/launcher/Launcher.qml) | QML 啟動器、搜尋、系統程式與已開啟視窗清單。 |
| [qml/overview/Overview.qml](../qml/overview/Overview.qml) | 左下半透明本機資訊卡。 |
| [qml/panel/TopPanel.qml](../qml/panel/TopPanel.qml) | 貼頂工作區、程式入口、時鐘、CPU 歷史、RAM 與電池狀態。 |
| [qml/session/LogoutPanel.qml](../qml/session/LogoutPanel.qml) | 登出確認面板。 |
| [qml/settings/SettingsPanel.qml](../qml/settings/SettingsPanel.qml) | QML 語言、圖片／shader 桌布與功能入口設定。 |
| [qml/shell.qml](../qml/shell.qml) | Quickshell root、IPC 輪詢／命令佇列與各面板可見性。 |
| [qml/style/Theme.qml](../qml/style/Theme.qml) | 共享灰黑／青綠色票、字型與狀態列高度。 |
| [qml/style/qmldir](../qml/style/qmldir) | 向 QML 宣告 Theme singleton。 |
| [qml/wallpaper/Wallpaper.qml](../qml/wallpaper/Wallpaper.qml) | 滿版等比例圖片桌布；無圖片時顯示下方的 shader 桌布。 |

### 資源、協定與封裝

| 檔案 | 用途 |
| --- | --- |
| [data/ludash.desktop.in](../data/ludash.desktop.in) | 產生 Wayland 登入工作階段 desktop entry 的範本。 |
| [data/plugins/fade/metadata.json](../data/plugins/fade/metadata.json) | 淡入外掛的 KPlugin／LuDash metadata 與程式庫名稱。 |
| [data/shaders/wallpaper.frag](../data/shaders/wallpaper.frag) | Dusk／Forest 桌布的 fragment shader。 |
| [data/shaders/wallpaper.vert](../data/shaders/wallpaper.vert) | 繪製全螢幕三角形的 vertex shader。 |
| [data/translations/en_US.json](../data/translations/en_US.json) | 英文語言包入口；空字典表示直接使用英文來源字串。 |
| [data/translations/zh_TW.json](../data/translations/zh_TW.json) | 繁中介面翻譯，中文內容集中在這裡。 |
| [data/wallpapers/README.md](../data/wallpapers/README.md) | 預設桌布的 AI 生成來源與使用說明。 |
| [data/wallpapers/florist.png](../data/wallpapers/florist.png) | 隨附的灰綠花店街景桌布，不含預先畫好的 UI。 |
| [packaging/arch/PKGBUILD](../packaging/arch/PKGBUILD) | Arch 本機來源封裝的 dependencies、build 與 package 步驟。 |
| [protocols/wlr-layer-shell-unstable-v1.xml](../protocols/wlr-layer-shell-unstable-v1.xml) | layer-shell wire protocol 定義；名稱沿用協定名稱，介面提供 v2 子集。 |

### 腳本與測試

| 檔案 | 用途 |
| --- | --- |
| [scripts/ludash-session](../scripts/ludash-session) | 實驗性的 EGLFS/KMS 獨立登入階段啟動腳本。 |
| [scripts/make-source.sh](../scripts/make-source.sh) | 打包可供 PKGBUILD 使用的本機來源，排除 Python cache。 |
| [scripts/security/check_sarif.py](../scripts/security/check_sarif.py) | 對 CodeQL SARIF 進行 fail-closed 檢查。 |
| [scripts/test-wayland.sh](../scripts/test-wayland.sh) | 隔離 Xvfb／runtime／設定，驗證 demo 或預設桌面並保存證據。 |
| [tests/DesktopTests.cpp](../tests/DesktopTests.cpp) | 原生工具、平鋪、翻譯、桌布與安全輸入的 Qt 行為測試。 |
| [tests/renderer/RenderTests.cpp](../tests/renderer/RenderTests.cpp) | 實際 GL／GLES context 與 production shader 測試。 |
| [tests/renderer/test_startup_failure.py](../tests/renderer/test_startup_failure.py) | 重現 context 不可用，要求退出碼 2 且沒有 abort。 |
| [tests/security/test_sarif_gate.py](../tests/security/test_sarif_gate.py) | 驗證 SARIF gate 會拒絕安全發現與缺失輸出。 |
| [tests/security/test_source_language.py](../tests/security/test_source_language.py) | 掃描 C++／QML 來源的中文字元政策。 |
| [tests/wayland/test_crash_detection.py](../tests/wayland/test_crash_detection.py) | 注入本次測試子程序 crash，確認工作階段失敗。 |
| [tests/wayland/test_shell_interactions.py](../tests/wayland/test_shell_interactions.py) | 在獨立 Xvfb 中點擊 Quickshell 並驗證 IPC／視窗操作。 |

### 文件

| 檔案 | 用途 |
| --- | --- |
| [docs/APPEARANCE.md](../docs/APPEARANCE.md) | 參考圖風格、狀態列／資訊卡與圖片桌布操作。 |
| [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) | 程序、模組、協定邊界與未完成項目。 |
| [docs/GRAPHICS.md](../docs/GRAPHICS.md) | GL／GLES context、shader、depth／stencil 與錯誤處理。 |
| [docs/INPUT_METHODS.md](../docs/INPUT_METHODS.md) | 語言包與 Fcitx5／IBus 測試，區分協定註冊與完整 bridge。 |
| [docs/PLUGINS.md](../docs/PLUGINS.md) | metadata 格式、外掛 SDK、載入方式與原生程式碼限制。 |
| [docs/SECURITY_CHECKS.md](../docs/SECURITY_CHECKS.md) | PR 資安／crash 工作流程、分支保護與本機檢查方式。 |
| [docs/TESTING.md](../docs/TESTING.md) | 簡明測試指引及錯誤判讀。 |
| [docs/TESTING_AND_FILES.md](../docs/TESTING_AND_FILES.md) | 本文件：完整測試流程、實測紀錄與逐檔用途。 |

## 12. 哪裡改什麼

| 想調整 | 從哪裡開始 |
| --- | --- |
| 狀態列外觀與色彩 | `qml/panel/TopPanel.qml`、`qml/components/Segment.qml`、`qml/style/Theme.qml`。 |
| 桌面資料卡 | `qml/overview/Overview.qml`；資料來源在 `system_status`。 |
| 平鋪與工作區 | `src/tiling/TilingLayout.cpp` 與 `src/compositor/WaylandCompositor.cpp`。 |
| OpenGL／GLES shader | `renderer` 模組與 `data/shaders/`。 |
| 增加翻譯 | `data/translations/` 與 localization／CMake resource 註冊。 |
| 增加原生外掛 | `CompositorPlugin.h`、fade 範例及自己的 metadata／功能目錄。 |
| 增加新 C++ 功能 | 建立 `include/LuDash/<feature>/`、`src/<feature>/`，並明列於 CMake。 |

修改後依影響範圍執行對應檢查；涉及 context、視窗生命週期、協定或 IPC 時，同時跑整合測試與 sanitizer。遠端 CI 是否完成，應以 GitHub 的實際 job 結果為準。
