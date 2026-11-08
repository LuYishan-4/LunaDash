# Graphics 與 Renderer 資源

[English](../en/GRAPHICS.md) · [繁中索引](README.md)

實際活動中的 compositor 使用 wlroots 選 renderer、allocator 與 scene output commit：

- `--graphics auto`：讓 wlroots 選可用 renderer。
- `--graphics opengl` / `gles`：在未另外設定 `WLR_RENDERER` 時要求 wlroots GLES2 renderer。
- Headless CI 一般使用 `WLR_RENDERER=pixman`。

Backend、renderer、allocator 或必要 global 建立失敗時，startup 會回報 subsystem diagnostic，而不是假裝成功。

## Toolkit-neutral render-element library

LunaDash 在 `src/compositor/renderer/` 保留不依賴 UI toolkit 的 OpenGL render-element library。`OpenGL` 要求呼叫端提供 current EGL OpenGL 3.3+ 或 OpenGL ES 3.0+ context；它不建立或擁有 context。活動中的 compositor scene 仍由 wlroots 負責，桌布由獨立的 Slint 殼層 process 顯示。

所有 project-owned raw GL 都在 `renderer/opengl/`。`Renderer` 負責 orchestration；`Shader`、`Program`、`Texture`、`Framebuffer` 用 RAII 管資源。這套 library 目前不是活動 wlroots compositor 的 scene renderer。

Built-in shader：

```text
src/compositor/renderer/opengl/shaders/
  Fullscreen.vert
  Wallpaper.frag
  Blur.frag
  Decoration.frag
```

CMake 會根據明確列出的 shader 檔案產生 C++ embedded resource header，因此 installed build 不依賴 source checkout 或工作目錄。

Software OpenGL 與 pixman 都不能證明特定 NVIDIA/AMD/Intel/ARM 實體 GPU、dmabuf 或 DRM/KMS 一定正常。

Shell（SlintLu）有自己的 rendering policy，見 [Shell rendering](SHELL_RENDERING.md)。

## Vulkan 選擇

`--graphics vulkan`（工作階段啟動器使用 `LUDASH_GRAPHICS=vulkan`）會要求 wlroots 的 Vulkan renderer。非空白 `WLR_RENDERER` 優先；`--graphics auto` 保留 wlroots 自動選擇。明確要求 Vulkan 卻初始化失敗時會回報錯誤並結束，不會將 pixman 回退誤報為 Vulkan。`lunadashctl status` 提供實際 `renderer` 與 `vulkanAvailable`；後者代表建置支援，並非 GPU 已通過驗證。

C11 選擇程式位於 `renderer/selection/RenderSelection.c`，僅連結 wlroots，不使用 Qt 物件。`LUDASH_ENABLE_VULKAN=OFF` 可關閉 Vulkan 整合；預設於 CMake 設定時偵測 wlroots API。Arch 需安裝 `vulkan-headers`、`vulkan-icd-loader` 與適合 GPU 的 Vulkan 驅動程式。其他 Linux 的相依套件由 `scripts/install-dependencies.sh` 處理，Nix 提供 loader 與標頭。Vulkan 需要可存取的 DRM render node 及 wlroots 所需的外部記憶體功能；只有 loader 或 Lavapipe 並不代表符合要求。

兩種 compositor renderer 共用 wlroots scene、allocator、dmabuf feedback 與依能力啟用的 explicit synchronization。GTK／Qt 客戶端使用的繪圖 API 不受 compositor 選擇限制。獨立 EGL OpenGL render-element library 仍限定 OpenGL。`LUDASH_SHELL_RENDERER=vulkan` 僅選擇殼層（SlintLu）的 Vulkan 路徑。建置 render-element target 需要 EGL 開發套件。

軟體渲染結果不代表實體 Vulkan GPU 已驗證。

2026-10-05 本機驗證：Arch Linux、wlroots 0.20.2、AMD Radeon 680M 搭配 RADV／Mesa 26.2.3，通過巢狀 Vulkan 工作階段的 GTK 3／GTK 4／Qt 映射、擷取與關閉測試。此結果僅涵蓋該 GPU 的 host Wayland 工作階段，不代表實體登入或其他驅動已驗證。
