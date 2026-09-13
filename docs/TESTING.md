# 如何測試 LuDash

先完成修改，再編譯一次。下面命令從專案根目錄執行。

## 1. 安裝 Arch 測試依賴

```sh
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative qt6-wayland quickshell qt6-translations mesa xorg-server-xvfb xorg-xauth python python-pillow
```

## 2. 編譯並執行自動測試

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

`ctest` 必須顯示 `100% tests passed`。Wayland 腳本會執行約 8 秒，最多再等 5 秒正常退出；成功行必須包含 `clean shutdown`，退出碼為 0。新版測試同時檢查程序狀態與正常關閉，子程序 crash 或逾時均回傳失敗。Pillow 會檢查 demo 視窗內部是否有實際畫面，拒絕只有單色外框的空白內容。

測試開始會移除上次的截圖／JSON，避免誤讀舊成功紀錄。`graphics-startup-failure` 會刻意限制 Mesa 為 OpenGL 3.2，預期 compositor 回報 context 初始化錯誤並以 2 退出，而非 SIGABRT。其餘圖形測試在自身程序內移除 Mesa GL／GLSL 版本 override。

```sh
echo "$?"
```

請在測試命令後立刻檢查退出碼。截圖與證據：

- `build/wayland-preview.png`：真正的 Quickshell／compositor 畫面。
- `build/wayland-state.json`：視窗幾何、可見性、程序失敗狀態。
- `build/wayland.log`：完整執行輸出。
- `build/Testing/Temporary/LastTest.log`：C++ 測試記錄。

## 3. 手動操作真正 Wayland 工作階段

在現有 Wayland 桌面的終端機執行：

```sh
QT_QPA_PLATFORM=wayland ./build/ludash-compositor --socket ludash-test
```

保留該終端機。另一個終端機開啟三個測試程式：

```sh
WAYLAND_DISPLAY=ludash-test QT_QPA_PLATFORM=wayland ./build/ludash-desktop --app files &
WAYLAND_DISPLAY=ludash-test QT_QPA_PLATFORM=wayland ./build/ludash-desktop --app notes &
WAYLAND_DISPLAY=ludash-test QT_QPA_PLATFORM=wayland ./build/ludash-desktop --app console &
```

逐項確認：

1. 視窗自動平鋪，避開頂部狀態列。
2. 筆記輸入文字後按關閉，選「取消」時內容及視窗保留；儲存後重新開檔，內容一致。
3. 主控台輸入 `printf 'hello\n'; exit 7`，應顯示 hello 與 Exit code 7。
4. 檔案管理器進入資料夾，再回上一層。
5. 點上方 1–4 切換工作區。按 Super+Shift+2 移動焦點視窗，再到工作區 2 查看。
6. Super+M 最小化後，在啟動器的「已開啟的視窗」還原；Super+Q 正常關閉。
7. 開啟設定切換桌布，約一秒內更新。

外層 KDE／GNOME 可能攔截 Super 組合鍵；先確認視窗取得焦點，或暫時解除衝突快捷鍵。上方工作區與啟動器可直接用滑鼠測試。

若目前主桌面是 X11，可使用以下 **host backend**；LuDash 內仍是 Wayland：

```sh
QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_egl ./build/ludash-compositor --socket ludash-test
```

## 4. 檢查記憶體錯誤

```sh
sudo pacman -S --needed clang
cmake -S . -B build-asan -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-asan -j4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build-asan --output-on-failure
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 LUDASH_BUILD_DIR="$PWD/build-asan" ./scripts/test-wayland.sh
```

出現 `AddressSanitizer`、`runtime error` 或非零退出碼都要視為失敗。此設定沒有測試 memory leak。

## 訊息判讀

- `SIGABRT` 搭配 `QMessageLogger::fatal`／`QQuickWindow::event`：可能是 Qt Quick 無法建立 context 後直接終止。附件中的呼叫堆疊屬於此路徑；檢查 log 與 `MESA_GL_VERSION_OVERRIDE`。新版接收 `sceneGraphError`，會輸出 `LuDash graphics initialization failed` 並回傳 2。
- `invalid method 8, object zwlr_layer_surface_v1`：Quickshell 發送 `set_layer`，需要 layer-shell v2。請重建新版 compositor；新版已加入該請求及版本協商。

- `Failed to initialize EGL display`：外層圖形 backend 的 EGL buffer integration 未初始化。舊腳本可能仍以其他 buffer 路徑顯示畫面，不能據此判定 GPU 加速正常。新版 Xvfb 測試指定 `xcb_egl`；請重新編譯後重跑。
- 若原因是 `There is no EGL_WL_bind_wayland_display extension`，Xvfb 環境無法使用該 EGL 客戶端 buffer 匯入路徑；本機測試仍可透過共享記憶體 buffer 顯示客戶端。這與 compositor 自身的 OpenGL／GLES context 建立失敗不同，須同時確認 context、截圖及正常退出檢查，不能把軟體測試當作硬體加速驗證。
- `App info not found for ludash-*`：host portal 找不到開發版的桌面應用程式登錄。這與 Wayland 視窗幾何檢查不同，目前不代表 portal 已受支援。
- 舊版結尾的 `程序已當機`：舊清理流程以 SIGTERM 結束子程序，Qt 也會以 Crashed 回報。新版改為傳送 Wayland close、等待正常退出並檢查結果；如果新版仍出現，請保留 log 當成真正失敗調查。

不要以登入畫面的獨立工作階段作為第一個測試。EGLFS/KMS 與真機 seat／VT 整合尚待驗證，先完成 nested 模式測試。

外觀參考與自訂桌布操作見 [APPEARANCE.md](APPEARANCE.md)。預設桌面截圖可用 `LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh` 產生。
