# OpenGL / OpenGL ES

`renderer` 模組管理 Qt Quick 與自訂渲染器的 context 邊界。Compositor 使用 OpenGL 3.3 Core 或 OpenGL ES 3.0 以上，未支援 GLES 2.0。

```sh
QT_QPA_PLATFORM=wayland ./build/ludash-compositor --graphics opengl
QT_QPA_PLATFORM=wayland ./build/ludash-compositor --graphics gles
```

`auto` 依 Qt 的 OpenGL module 類型選擇預設；不是 GPU 故障後的自動重新啟動機制。指定 API 無法建立時應回報錯誤，不會把 desktop OpenGL 假稱為 GLES。

- `RenderBackend` 在 QApplication 建立前設定 QSurfaceFormat 與 Qt Quick 圖形 API。
- 保留 24-bit depth／8-bit stencil，供 Qt Quick 的不透明項目排序與裁切使用；關閉 depth buffer 可能讓父項目的外框蓋住客戶端內容。
- `WallpaperItem` 提供 Qt Quick framebuffer item。
- `WallpaperRenderer` 在 render thread 取得 `QOpenGLContext::currentContext()`，檢查實際 API 與版本，使用該 context 的 `QOpenGLExtraFunctions`。
- `data/shaders/wallpaper.vert` / `.frag` 是真正編譯與連結的 GLSL。桌面版加入 `#version 330 core`；GLES 加入 `#version 300 es` 與 precision 宣告。
- shader program、VAO 與 FBO 都在 render thread 的 context 下使用；渲染完成後重設 Qt Quick 需要的 GL 狀態。GUI 與 render thread 的驗證狀態以 atomic 欄位交換。
- Quickshell 提供桌面 UI 與圖片桌布；切換至 Dusk／Forest 後，透明背景 surface 顯示 compositor shader 桌布；Quickshell 自身與 LuDash 是不同程序、不同 context。

驗證：

```sh
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build -R graphics-contexts --output-on-failure
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

JSON 證據含實際 `graphicsApi`、context 版本、`shaderReady` 與 `graphicsFailed`。Xvfb／Mesa 驗證的是軟體驅動，不能代替 NVIDIA／AMD／Intel／ARM 的真機相容性測試。

圖形初始化失敗會透過 `QQuickWindow::sceneGraphError` 記錄原因並以退出碼 2 結束；無效的 `--graphics` 參數也以 2 結束。`graphics-startup-failure` 測試刻意以 Mesa 3.2 限制重現無法建立 3.3 context 的情境，確認不再呼叫 abort。

如果環境有 `MESA_GL_VERSION_OVERRIDE=3.2`，它可能阻止 EGL 建立所需的 OpenGL 3.3 context。自動測試僅在自身程序內移除 Mesa GL／GLSL override，不會修改使用者設定。手動啟動可用 `env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE QT_QPA_PLATFORM=wayland ./build/ludash-compositor --graphics opengl`。

參考：[Qt FBO render thread 與 context 規則](https://doc.qt.io/qt-6/qquickframebufferobject-renderer.html)、[QSurfaceFormat](https://doc.qt.io/QT-6/qsurfaceformat.html)。
