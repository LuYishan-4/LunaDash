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
