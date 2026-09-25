# 原始碼架構

[English](../en/ARCHITECTURE.md) · [繁中索引](README.md)

LunaDash 是以 C++20、C11、wlroots、Qt 6 與 Quickshell/QML 組成的 Wayland 桌面。**wlroots 是實際合成器後端**；Quickshell/QML 負責桌面 Shell，Qt Widgets 用於原生工具，Qt Wayland 是 client 端依賴，不是 compositor server。Arch Linux 是主要開發平台。巢狀或軟體繪圖測試成功，不等於所有實體 GPU／登入工作階段都已驗證。

## 原始碼分區

```text
src/
  compositor/
    animation/       wlroots scene 視窗動畫
    capture/         截圖與擷取協調
    client/          compositor 擁有的 client 狀態
    input/           seat、鍵盤、指標、輸入法
    ipc/             本機 JSON control socket
    layout/          共用 WindowLayout 介面與策略建立
    plugins/         Plugin SDK 2 原生 hook 載入
    renderer/        renderer 編排與 render elements
      opengl/        GL dispatch、shader/program/texture/FBO 與 pass
    session/         啟動、子程序環境與 launch policy
    stacking/        可由 plugin 選用的重疊視窗策略
    tiling/          預設 bounded split tree 與 grouped rows
    wayland/         wlroots runtime、output、surface
    window/          window rules、frame、switcher
    xwayland/        驗證過的按需 X11 相容層
  config/            preferences、翻譯、plugin catalog
  core/              共用 contract、listener 與 C plugin API
  ctl/               lunadashctl
  desktop/           預設程式、設定、系統／音訊／網路等桌面服務
  service/portal/    FileChooser portal
  shell/             shell module runtime 與媒體 helper
```

C++ 檔名使用 PascalCase 的 `.hpp/.cpp`，資料夾使用小寫 domain；C 介面維持 `.h/.c` 與既有 `ludash_` symbol。每個可執行入口的 `Main.cpp` 只負責把控制交給對應 module。

## 相依與生命週期

相依方向大致是 `core → config → runtime domains`。Core 不可 include 上層模組；desktop 不可直接依賴 compositor internals；renderer 不可依賴 desktop UI。專案用 `"compositor/renderer/Renderer.hpp"` 這種 source-root include，避免深層相對路徑。

`WaylandCompositor` 擁有工作階段整合；`Runtime.cpp` 管 setup/teardown，`Output.cpp` 管 output，`Surface.cpp` 管 xdg/layer surface，`input/Input.cpp` 管輸入事件與 input-method bridge。物理鍵盤 map 與 repeat 由 `Keyboard.cpp` 套用。

`core/templates/WaylandListener.hpp` 統一 wl_listener 的註冊與移除，owner 銷毀前先 disconnect。視窗關閉動畫使用保留的 scene snapshot，不保留已失效 surface callback。實際 wlroots 視窗動畫由 `SceneWindowAnimations` 驅動。

## 視窗配置

`src/compositor/layout/WindowLayout.hpp` 是共用 contract。預設為 `TilingLayout`；Plugin SDK 2 的 `window-layout` replacement 可以選擇 `StackingLayout`。Host 仍擁有 window ID、workspace、focus、minimize/maximize 與 client configure；plugin 不取得裸 compositor 指標。

## Plugin SDK 2

Plugin metadata discovery 在 `src/config/plugins/`，因此設定 UI 可以檢視 plugin 而不用 include compositor loader。Plugin 分成：

- `effect`：C11/C++20，透過版本化 C ABI 的同步 JSON hook。
- `quickshell`：QML/JavaScript visual slot。
- `opengl`：由 SDK 透過 `qsb` 建置的 Quickshell shader package。

原生 plugin 預設關閉，而且 metadata / build receipt **不是 sandbox 或簽章**。開發版不承諾穩定的原生 binary ABI。

## Rendering

活動中的 compositor 由 wlroots 擁有 renderer、allocator 與 scene。一般 `--graphics auto` 讓 wlroots 選擇；`opengl`/ `gles` 目前都會要求 wlroots GLES2 renderer；headless CI 明確使用 pixman。

應用程式毛玻璃沿用這條 scene 繪圖路徑。`renderer/blur/WindowGlass` 在受管理視窗 tree 下方維護不接收輸入的同層背景節點；`SceneBackdrop.c` 只擷取堆疊順序較低的 scene 內容，再透過 wlroots render pass 模糊。用戶端 buffer 保持清晰並畫在其上。輸出提交前更新背景快取，暫時動畫透明度仍由既有動畫程式控制。內容 commit revision 及 scene 幾何變動會更新快取；只有 frame callback 的 commit 及視窗自身的背景節點不會觸發更新。Scene 銷毀 listener 會在 renderer 結束前釋放保留的 buffer。沿用現有外觀偏好設定，全螢幕、utility 及桌面視窗不套用此效果。

另外保留一套 Qt render-element library 於 `src/compositor/renderer/`。所有專案自有 raw OpenGL 都在 `renderer/opengl/`；`GLDispatch.c` 負責 function loading，`Shader`、`Program`、`Texture`、`Framebuffer` 以 RAII 管資源。這套 Qt GL library 有獨立測試，但**目前不是 wlroots scene 的主 renderer**。

內建 shader 明確列在 CMake 中並嵌入 `:/LunaDash/renderer/shaders/`，安裝後不依賴 source checkout。SDK 2 的 OpenGL plugin 是另一條 Quickshell visual-slot 路徑，不能任意改 application buffer 或 wlroots output framebuffer。

## 新增功能時

1. 選既有 domain，新增同一職責的 interface/implementation。
2. 明確決定 lifetime owner。
3. 將 source 明確加入對應 CMake target。
4. QML 放 `qml/<feature>/`；翻譯放外部 catalog。
5. 只有真的需要 extension boundary 時才新增 manager/provider 抽象。
6. renderer、input、Wayland lifetime 與 plugin contract 都要補對應測試。

## 驗證與限制

```sh
python3 scripts/check-source-layout.py
python3 tests/architecture/test_source_layout.py
```

架構檢查會拒絕錯誤命名、舊 renderer 結構、違反 include 方向、漏列的 CMake source、錯放 shader 與 renderer 外的 raw OpenGL。

CI 會覆蓋 source layout、protocol globals、xdg lifecycle、headless compositor、on-demand XWayland、renderer 資源與 plugin SDK regression。實體 GPU、Chrome/Zed、Fcitx 候選視窗、實體輸入、多螢幕與 display-manager login 仍需真實工作階段測試。FileChooser portal 也不是完整 PipeWire ScreenCast portal。

延伸閱讀：[測試與原始碼地圖](TESTING_AND_FILES.md)、[圖形](GRAPHICS.md)、[輸入法](INPUT_METHODS.md)、[Plugin SDK 2](PLUGINS.md)。

## NyxNiri 桌面整合、Orbit 與動態桌布

參照布局沿用 `panel`、`overview`、`launcher` 與 `dock` 模組。`qml/panel/` 負責面板顯示、`qml/overview/` 負責控制中心；`qml/settings/pages/appearance.qml` 與通用模組編輯器修改同一份面板設定。`data/modules/registry.json` 宣告經驗證的欄位及預設值，`src/shell/modules/` 管理正規化、儲存及檔案監看，`src/config/desktop/` 繼續管理 Dock 顯示等桌面偏好。底部 Dock 預設關閉，已有的明確偏好保持有效。

面板工作區篩選只是顯示選項，不變更 compositor 的工作區配置。置中啟動器、視窗標題及 CPU／記憶體資訊使用既有 shell 狀態。作者風格配置維持使用 LunaDash 原有 JSON／設定介面供使用者編輯，不引入 Niri 設定解析器、替代 compositor 或第二套桌面 shell。首次安裝引導擴充原有安裝腳本並選擇性匯入應用程式設定，compositor 設定仍歸 LunaDash 管理。

設定、快捷鍵、相依套件及驗證界線請參閱[桌面整合說明](NYXNIRI_DESKTOP.md)；可選的首次安裝流程請參閱[工作階段安裝](LOGIN_SESSION.md)。

檔案管理器啟動透過外部 files 角色解析，預設為 Dolphin。退役的 `desktop/filemanager`、`desktop/fileoperations` 實作與建置目標已移除。Portal 選擇器與圖示歸屬 `service/portal`；Shell 圖片選擇器及螢幕分享選擇器各自保留。
