# Graphics 與 Renderer 資源

[English](../en/GRAPHICS.md) · [繁中索引](README.md)

實際活動中的 compositor 使用 wlroots 選 renderer、allocator 與 scene output commit：

- `--graphics auto`：讓 wlroots 選可用 renderer。
- `--graphics opengl` / `gles`：在未另外設定 `WLR_RENDERER` 時要求 wlroots GLES2 renderer。
- Headless CI 一般使用 `WLR_RENDERER=pixman`。

Backend、renderer、allocator 或必要 global 建立失敗時，startup 會回報 subsystem diagnostic，而不是假裝成功。

## Qt render-element library

LunaDash 同時保留 `src/compositor/renderer/` 的 Qt render-element library。`OpenGL` 要求 current OpenGL 3.3+ 或 OpenGL ES 3.0+ context；它不自己建立 Qt context。

所有 project-owned raw GL 都在 `renderer/opengl/`。`Renderer` 負責 orchestration；`Shader`、`Program`、`Texture`、`Framebuffer` 用 RAII 管資源。**這套 library 目前不是活動 wlroots compositor 的 scene renderer。** Image wallpaper 由獨立 Quickshell process 顯示。

Built-in shader：

```text
src/compositor/renderer/opengl/shaders/
  Fullscreen.vert
  Wallpaper.frag
  Blur.frag
  Decoration.frag
```

CMake 明確把它們嵌入 `:/LunaDash/renderer/shaders/`，因此 installed build 不依賴 source checkout 或工作目錄。

## 測試

```sh
cmake -S . -B build -G Ninja -DLUDASH_BUILD_RENDERER_TESTS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -R '^lunadash-renderer$'

QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a \
  ./build/lunadash-renderer-test --opengl
```

Default renderer test 會驗 embedded resources、missing-context diagnostic，以及 creation/compile/link 失敗後是否釋放 partial allocation；`--opengl` 會以 software GL compile 所有 shipped shader 並實際 draw 到 framebuffer。

wlroots session test 是另一條 pixman 路徑：

```sh
LUDASH_TEST_NO_SHELL=1 LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
python3 tests/renderer/test_startup_failure.py build/lunadash-compositor
```

Software OpenGL 與 pixman 都不能證明特定 NVIDIA/AMD/Intel/ARM 實體 GPU、dmabuf 或 DRM/KMS 一定正常。Host Wayland 可用 `LUDASH_TEST_HOST_WAYLAND=1` 額外測試。

Quickshell 有自己的 rendering policy，見 [Shell rendering](SHELL_RENDERING.md)。

## Vulkan 選擇

`--graphics vulkan`（工作階段啟動器使用 `LUDASH_GRAPHICS=vulkan`）會要求 wlroots 的 Vulkan renderer。非空白 `WLR_RENDERER` 優先；`--graphics auto` 保留 wlroots 自動選擇。明確要求 Vulkan 卻初始化失敗時會回報錯誤並結束，不會將 pixman 回退誤報為 Vulkan。`lunadashctl status` 提供實際 `renderer` 與 `vulkanAvailable`；後者代表建置支援，並非 GPU 已通過驗證。

C11 選擇程式位於 `renderer/selection/RenderSelection.c`，僅連結 wlroots，不使用 Qt 物件。`LUDASH_ENABLE_VULKAN=OFF` 可關閉 Vulkan 整合；預設於 CMake 設定時偵測 wlroots API。Arch 需安裝 `vulkan-headers`、`vulkan-icd-loader` 與適合 GPU 的 Vulkan 驅動程式。其他 Linux 的相依套件由 `scripts/install-dependencies.sh` 處理，Nix 提供 loader 與標頭。Vulkan 需要可存取的 DRM render node 及 wlroots 所需的外部記憶體功能；只有 loader 或 Lavapipe 並不代表符合要求。

兩種 renderer 共用 wlroots scene、allocator、dmabuf feedback 與依能力啟用的 explicit synchronization。GTK／Qt 客戶端使用的繪圖 API 不受 compositor 選擇限制。保留的 Qt OpenGL 特效函式庫仍限定 OpenGL。`LUDASH_SHELL_RENDERER=vulkan` 僅選擇 Quickshell 的 Qt Quick Vulkan 路徑。

GPU 主機可執行：

```sh
LUDASH_TEST_RENDERER=vulkan LUDASH_TEST_HOST_WAYLAND=1 \
  dbus-run-session -- python3 tests/wayland/test_toolkits.py build
```

此測試檢查 GTK 3、GTK 4 與 Qt 的映射、擷取及關閉，並保留狀態、紀錄與截圖。未設定環境變數時使用 headless pixman。CI 另行驗證 Vulkan 失敗處理與 shell 選擇；軟體測試不代表實體 Vulkan GPU 已驗證。

2026-10-05 本機驗證：Arch Linux、wlroots 0.20.2、AMD Radeon 680M 搭配 RADV／Mesa 26.2.3，通過巢狀 Vulkan 工作階段的 GTK 3／GTK 4／Qt 映射、擷取與關閉測試。此結果僅涵蓋該 GPU 的 host Wayland 工作階段，不代表實體登入或其他驅動已驗證。
