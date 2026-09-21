# Shell Rendering

[English](../en/SHELL_RENDERING.md) · [繁中索引](README.md)

Quickshell 與 compositor 是不同 process，也有不同 rendering path：

- Compositor：wlroots renderer/allocator/scene。
- Shell：Qt Quick，繼承 LunaDash session rendering environment。

`SessionEnvironment` 在 `QSG_RHI_BACKEND` 與 `QT_QUICK_BACKEND` 都沒有指定時，預設讓 Qt Quick 使用 `opengl`。

要在 host Wayland 內比較 software Shell：

```sh
QT_QUICK_BACKEND=software LUDASH_TEST_HOST_WAYLAND=1 \
  LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
```

Software rendering 可能提高 CPU 使用量，GPU-only QML effect 需要 fallback。

專案保留 `shell/runtime/ShellRenderer` compatibility helper，但**目前 wlroots startup path 沒有呼叫它**，因此不能看到 `LUDASH_SHELL_RENDERER=auto` 就假設 NVIDIA auto-detection 正在活動。先前 NVIDIA/Qt 的 descriptor/fence observation 是歷史背景，不是當前所有 driver 的證據。

像：

```text
QProcess: Cannot create pipe (Too many open files)
```

代表 resource exhaustion，不應直接解讀成 executable 缺失。

## Built-in GLSL

獨立的 Qt render-element library 將 `src/compositor/renderer/opengl/shaders/` 的 shader 明確嵌入 resource。`ShaderAssetLoader` 可辨識 vertex/fragment/geometry/compute/tessellation suffix 與 stage pragma，並在來源缺 version 時補上適合版本。

Renderer tests 驗 relocation、software OpenGL、failure cleanup，但不能證明所有 GPU/fence leak 都不存在，也不能表示這些 Qt effects 已接到 wlroots scene。

更多：[Graphics](GRAPHICS.md)、[Effects](EFFECTS.md)、[Testing](TESTING_AND_FILES.md)。
