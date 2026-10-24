# Slint 與 wlroots 遷移

[English](../en/SLINT_WLROOTS_MIGRATION.md) · [繁中索引](README.md)

## 目標架構

- 獨立的 `LunaDash-Shell` repo 維持原樣；Quickshell/QML 是獨立 Wayland client，可以使用自己的 Qt runtime。
- LunaDash compositor 與內建 service 不應連結 Qt，也不應處理工具包專屬的 surface。
- wlroots 負責 compositor、Wayland protocol、輸入路由、輸出與場景合成。不同 Shell 透過標準 Wayland protocol 連線。
- LunaDash 自有的 Settings、Welcome、Package Manager 與 portal UI 使用 Slint Wayland client。
- 設定、程序、網路與 D-Bus 整合使用標準 C++ 與 Linux 系統函式庫；wlroots 不負責取代這些服務。
- Quickshell 應在 compositor／基礎套件邊界保持選用。獨立 `LunaDash-Shell` 安裝程式可以繼續要求 Quickshell。

## 遷移順序

1. 建立工具包中立的 Wayland 相容矩陣，先涵蓋 `xdg-shell` 與 `wlr-layer-shell`，再依 client 需求處理 activation、decoration、foreign-toplevel/workspace、screencopy、text-input 與 virtual keyboard。
2. 將本 repo 的 Welcome、設定介面、Package Manager 與 portal picker 移為 Slint Wayland clients。檔案路徑留在 LunaDash 管理，絕不透過 shell 執行選取的路徑。
3. 移除 compositor renderer 中的 Qt Quick／Qt OpenGL wrapper；wlroots 負責合成，特定效果才使用專案自有 OpenGL。
4. 在非 UI 模組中，以 C++ 標準函式庫與 Linux service API 取代 Qt utility API。
5. 將 native QtTest 改為 Qt-free 測試；獨立 Shell repo 的 QML 測試不在本次範圍。
6. 移除 compositor/service 對 Qt 的連結，並讓 Quickshell 成為可選的外部 Shell 相依套件。

## 相容邊界

Compositor 以 Wayland protocol 接收 shell surfaces，client 可由 Qt Quick、Slint 或其他工具包實作。工具包本身不保證 client 使用特定 protocol：一般 Slint 視窗使用 `xdg-shell`，而 Slint panel 需要 `wlr-layer-shell` client 支援或退回 `xdg-shell`。Quickshell/Qt client 也只會在其使用 compositor 已實作的 protocol 時相容。

無法保證相容所有 Shell，因為有些 Shell 依賴 compositor 專屬 IPC 或 extension。應維護能力矩陣，並用代表性 client 驗證 protocol 行為；LunaDash 專屬控制應透過選用 IPC 提供，不應成為顯示或輸入的前提。

目前 compositor 註冊 `xdg-shell` v3、`wlr-layer-shell` v4、foreign-toplevel management、`wlr-screencopy`、可用時的 ext foreign-toplevel/window capture、`xdg-output`、idle inhibition、input-method-v2、text-input-v3 與 virtual-keyboard。安裝的 wlroots headers 支援時也會註冊 `xdg-activation-v1`，並驗證來源 seat client 的有效 serial。註冊 global 只代表目前程式碼基線，不代表 client 行為已完全相容。

| Client 能力 | 目前提供 | 相容界線 |
| --- | --- | --- |
| 一般視窗與 popup | `xdg-shell` v3 | 可依實作生命週期建立、調整大小、最大化與關閉。 |
| Panel、背景與 Shell overlay | `wlr-layer-shell` v4 | Qt Quick、Slint 等 client 必須支援此 protocol 才能建立 layer role；一般視窗不會自動取得該 role。 |
| Buffer 與 subsurface | `wl_compositor` v5、subcompositor、viewporter、shm，以及依 renderer 能力提供的 linux-dmabuf／explicit sync | buffer 匯入受協商格式與 renderer 能力限制，client 應保留 shm fallback。 |
| 視窗查詢與擷取 | 可用時提供 foreign-toplevel/capture globals；`wlr-screencopy` v3 | 可用 globals 依 wlroots 版本而異，不代表每個 portal 支援所有擷取路徑。 |
| 文字輸入與虛擬鍵盤 | input-method-v2、text-input-v3、virtual-keyboard-v1 | 第三方輸入法仍需要自己的 Wayland client 整合。 |
| 視窗啟用 | headers 支援時提供 `xdg-activation-v1` | 只接受有效來源 serial 及已映射的非 utility 視窗；舊 wlroots client 應保留 fallback。 |
| Server-side decoration、session lock、標準 workspace control | 尚無專用 globals | 依賴這些介面或 compositor 私有 IPC 的 Shell 需要 fallback 或另外實作 protocol。 |

矩陣列出 protocol 能力及缺口。宣稱相容特定 Shell 前，應先補上 xdg-shell 和 layer-shell 的行為測試；只查 global 是否註冊並不足夠。

## 遷移選擇

- JSON 與設定：`nlohmann/json`、XDG 設定檔。
- D-Bus：GLib `GDBus`。
- HTTP：`libcurl`。
- 程序與 timer：`posix_spawn` 整合 Wayland event loop。
- 場景合成：可用時使用 wlroots scene/render API；必要的自有效果使用 OpenGL。
- Shell 相容：標準 Wayland protocol，加上選用的 LunaDash IPC。

若實作遇到會明顯改變以上選擇的 API 限制，先整理取捨再更改。

## 目前狀態

LunaDash 自有的 Welcome、Package Manager、FileChooser 與 ScreenCast chooser 已是 Slint/Winit Wayland clients；portal backend 以 GLib GDBus/GSubprocess 處理服務請求。runtime 翻譯改由原生 JSON loader 載入，語系偏好寫入原生 store，舊 INI 語系只在遷移時作 fallback，重設時會忽略舊值。內部 shared libraries 現在會隨 shared build 安裝，程式使用相對 runtime paths。

多個 compositor/service target 已改用標準 C++、nlohmann/json、GIO、libcurl、POSIX process runner 與 wlroots event loop。OpenGL render-element 使用呼叫端提供的 EGL context；Qt Quick-only wallpaper/blur wrapper 已移除。Shell module、Orbit settings、media、network、system status、weather、update checks、keyboard 與 shortcut service 已有 Qt-free target。桌面偏好 API 與 keyboard preference application 現在使用 nlohmann/json 和標準字串；GLib `GKeyFile` 會一次性匯入舊 desktop/setup/wallpaper 設定，並保留原 INI 檔。

LunaDash 本身的建置已不再尋找或連結 Qt。Compositor、plugin manager 與 catalogue、settings API、視窗 layout、桌面 service、內建 Slint client、portal backend 與 native 測試都改用標準 C++、nlohmann/json、GLib/GIO、libcurl、POSIX process runner 與 wlroots event loop。`src/core/` 的小型工具取代原本的 Qt 值型別：保留 layout 既有「右／下邊界含端點」語意的整數幾何（`Rect`、`Size`、`Point`）、寬鬆 JSON 讀取、UTF-8/URL 工具、原子化寫檔、暫存目錄與 printf 形式的 log。Compositor 在自己的 `poll` 迴圈中驅動 GLib 預設 main context，因此 GIO file monitor、D-Bus 呼叫與非同步 I/O 不需要工具包 dispatcher，也會在 compositor thread 執行。

Native C++ 測試使用 `tests/support/TestHarness.hpp`；D-Bus fixture 改用 GDBus。`tests/architecture/test_no_qt.py` 會拒絕 `src/`、`tests/` 與建置設定中的 Qt header、型別、macro 與 CMake 連結。刻意保留、且不屬於 LunaDash binary 的只有兩項：獨立的 `LunaDash-Shell`（Quickshell/QML）及其 QML CI，以及 plugin SDK 為該 Shell 載入的 OpenGL plugin 產生 `.qsb` shader 的可選步驟。Compositor 傳給第三方 Qt 應用程式的環境變數屬於設定，不是連結。

## 驗證狀態與限制

- `cmake --build build -j2` 成功；`ctest --test-dir build --output-on-failure` 通過 37/37。
- 翻譯檢查的 `zh_TW`、`zh_CN`、`ja_JP` 靜態字串覆蓋率皆為 100%，重複 key 為 0；QML design/action 稽核通過。
- Pixman headless toolkit flow 讓 GTK 3、GTK 4、Qt Wayland 與 Slint clients 完成映射、擷取及關閉。套件管理器 screenshot 位於 build 證據目錄。
- staged `/usr` 安裝中，所有安裝的 executable 都能在沒有 `LD_LIBRARY_PATH` 時解析依賴；Slint UI、control client 與 portal client 沒有 Qt runtime。
- Host nested Wayland 嘗試在 wlroots swapchain test 失敗，Pixman 與 GLES2 都如此；實體 GPU／登入工作階段尚未完成視覺驗證。

## 完成條件

- Compositor/service 的乾淨建置不搜尋、include、連結或下載 Qt。
- Compositor 不連結任何 UI 工具包，並透過 Wayland protocol 支援 Qt/Quickshell、Slint 與其他 Shell client。
- 基礎 compositor 套件不強制安裝 Quickshell；獨立 `LunaDash-Shell` 可以維持自己的 Qt runtime。
- LunaDash 自有 Settings、Welcome 與 portal UI 使用 Slint。
- wlroots 擁有 compositor protocol、輸入與輸出生命週期。
- 清楚區分 CI 設定平台與實際完成驗證的平台。
