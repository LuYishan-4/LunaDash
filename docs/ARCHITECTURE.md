# LuDash 架構

```text
Linux / existing Wayland host / EGLFS-KMS
  ludash-compositor (C++20, Qt Wayland Compositor, OpenGL)
    xdg-shell: native application windows
    layer-shell v2 subset: Quickshell wallpaper/panel/overview/overlays
    text-input v2/v3 + Qt input-method protocol
    master/stack tiling, four workspaces, focus, window effects
    local user-only JSON control socket
      ludashctl -> Quickshell status and controls
    quickshell --path qml/shell.qml
      wallpaper / panel / overview / launcher / settings / session
    ludash-desktop --app <id>
      files / notes / console / monitor / packages / plugins / settings
```

桌面外殼使用 Quickshell／QML。獨立的內建工具目前使用 C++ Qt Widgets，可與其他 Wayland 應用程式一起平鋪；compositor 以 OpenGL 合成所有表面。Quickshell Scene Graph 使用 OpenGL，桌布由 C++ render-thread context 中的 GLSL vertex／fragment shader 繪製。

每項 C++ 功能有 `include/LuDash/<feature>/` 與 `src/<feature>/` 目錄。標頭僅宣告型別與介面，實作在 `.cpp`；入口點放 `src/entrypoints/`。QML 每項功能放 `qml/<feature>/`。翻譯僅放 `data/translations/`。CMake 明確列出來源檔。

- `renderer`：OpenGL 3.3／GLES 3.0 context、FBO、GLSL 與跨執行緒狀態。
- `compositor`、`tiling`：Wayland 視窗生命週期與主欄／堆疊幾何。
- `layer_shell`：Quickshell 背景、頂部狀態列、資訊卡、overlay；v2 子集（含 set_layer），固定單一輸出，layer popup 尚未實作。
- `ipc`：user-only Unix socket，大小限制及連線逾時；控制器僅接受列出的指令。
- `input_method`、`localization`：輸入法協定與語言資源。
- `plugins`、`fade_plugin`、`plugin_settings`：metadata 探索、明確啟用、native effect 範例與設定。
- `packages`：pacman 唯讀查詢，異動交由終端機與 sudo/pacman 原生確認；不執行 `--noconfirm`、`-Sy` 或任意 shell。
- `file_manager`、`notes`、`console`、`system_monitor`、`welcome`、`launcher`、`settings`：獨立工具。

Qt 資源初始化及 Wayland scanner 產生的 C 協定檔遵循上游要求使用全域符號，其餘專案 C++ 使用 `LuDash` namespace。

這是單輸出的開發版本，不是完整 KDE 替代品。尚缺 XWayland、完整 layer-shell（任意 exclusive zone、popup、所有雙緩衝狀態）、多螢幕、螢幕鎖定、portal、PipeWire 擷取、網路／音訊／電源服務與完整 IME bridge。面板可用區域目前固定預留頂部 28px 與平鋪間距，符合本專案隨附的 Quickshell 版面。

EGLFS/KMS 獨立登入工作階段仍待 seat、VT 與 GPU 真機驗證。優先使用 nested 模式。
