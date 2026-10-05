# Shell Rendering

[English](../en/SHELL_RENDERING.md) · [繁中索引](README.md)

Quickshell 與 compositor 是不同 process，也有不同 rendering path：

- Compositor：wlroots renderer/allocator/scene。
- Shell：Qt Quick，繼承 LunaDash session rendering environment。

Renderer 政策只套用於 Quickshell，不再向所有應用程式強制設定 OpenGL。

要在 host Wayland 內比較 software Shell：

```sh
QT_QUICK_BACKEND=software LUDASH_TEST_HOST_WAYLAND=1 \
  LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
```

Software rendering 可能提高 CPU 使用量，GPU-only QML effect 需要 fallback。

`shell/runtime/ShellRenderer` 現在於啟動 shell 時套用。NVIDIA 自動選擇軟體模式是相容性預設，可由使用者明確選擇 OpenGL 或 Vulkan 覆寫；不代表每一版本的驅動都有相同問題。

像：

```text
QProcess: Cannot create pipe (Too many open files)
```

代表 resource exhaustion，不應直接解讀成 executable 缺失。

## Built-in GLSL

獨立的 Qt render-element library 將 `src/compositor/renderer/opengl/shaders/` 的 shader 明確嵌入 resource。`ShaderAssetLoader` 可辨識 vertex/fragment/geometry/compute/tessellation suffix 與 stage pragma，並在來源缺 version 時補上適合版本。

Renderer tests 驗 relocation、software OpenGL、failure cleanup，但不能證明所有 GPU/fence leak 都不存在，也不能表示這些 Qt effects 已接到 wlroots scene。

更多：[Graphics](GRAPHICS.md)、[Effects](EFFECTS.md)、[Testing](TESTING_AND_FILES.md)。


## 共用 Surface 設計

1.0.1a 的 Dashboard、Settings、桌布 picker 與 portal wrapper 使用同一套 strong/glass/hairline/radius 規則。Dashboard telemetry 使用 bounded layout，不應靠絕對文字座標堆疊；Settings 可切換最大化而不改 page contract；desktop widget plugin 直接繪製在 wallpaper Background surface。

啟動 Quickshell 時會套用 `shell/runtime/ShellRenderer`。`LUDASH_SHELL_RENDERER` 支援 `auto`、`opengl`、`vulkan`、`software`。Auto 保留明確的 Qt 渲染選擇，否則在偵測到 NVIDIA 時使用軟體模式，其餘使用 OpenGL；此政策僅修改 shell 子程序的環境，GTK／Qt 應用程式自行選擇繪圖 API。
