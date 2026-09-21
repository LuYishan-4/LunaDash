# C 核心與 C++ 整合

[English](../en/C_CORE.md) · [繁中索引](README.md)

LunaDash 以 C11 處理範圍明確的低階工作，以 C++20 管物件生命週期與 Qt/wlroots 整合；桌面 Shell 使用 Quickshell/QML。

| C implementation | 職責 | C++ owner |
| --- | --- | --- |
| `src/compositor/renderer/opengl/GLDispatch.c` | 載入需要的 OpenGL entry points 與 bounded diagnostics | `OpenGL` 使用 Qt render-thread 的 current context |
| `src/compositor/tiling/TilingGeometry.c` | 驗證尺寸並計算 column/member geometry | `TilingLayout` |
| `src/desktop/system/SystemMetrics.c` | 解析 CPU/memory counter 並計算 bounded percentage | `SystemStatus` / `SystemMonitor` |

C symbol 保留 `ludash_` 前綴與既有 C type 名稱。Header 在 C++ 中以 C linkage 暴露，但 C compiler 仍看到相同 ABI。每個 C implementation 都必須明確列入 CMake。

Shader compilation、program linking、texture/FBO ownership 是 `renderer/opengl` 的 C++ RAII；並不存在第二套 C renderer。實際 compositor renderer/allocator 與 Wayland protocol lifetime 仍由 wlroots 擁有。

Renderer failure-injection test 驗 partial cleanup；software OpenGL path 驗 embedded shader compile/draw。更多內容見 [Graphics](GRAPHICS.md) 與 [Testing](TESTING_AND_FILES.md)。
