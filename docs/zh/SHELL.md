# 桌面殼層

[English](../en/SHELL.md) · [繁中索引](README.md)

面板、Dock、啟動器、儀表板、通知、鎖定畫面與殼層設定來自獨立的
[LunaDash-Shell](https://github.com/LunaDash-DE/LunaDash-Shell) 儲存庫。殼層以 Slint 撰寫，
由 [SlintLu](https://github.com/LunaDash-DE/SlintLu) 執行：SlintLu 會把 Slint 檔案變成
Wayland layer-shell 視窗，並提供桌面服務（音訊、網路、MPRIS、通知、系統匣、polkit、PAM、
閒置、剪貼簿）。合成器只透過 Wayland 與控制 socket 與殼層溝通，LunaDash 本身不連結任何殼層工具組。

## 版本與安裝

`packaging/shell-revision` 固定殼層的 commit，`packaging/slintlu-revision` 固定與之一起測試過的
SlintLu commit。`scripts/fetch-shell.sh` 與 `scripts/fetch-slintlu.sh` 會下載這兩個確切的 commit。

- CMake 會把 `LUDASH_SHELL_SOURCE_DIR` 中的殼層檔案安裝到 `share/lunadash/shell`。舊名稱
  `LUDASH_QML_SOURCE_DIR` 仍然可用。
- 指定 `-DLUDASH_SLINTLU_SOURCE_DIR=<checkout>` 時，CMake 會以 Cargo 建置 SlintLu，並安裝為
  `libexec/lunadash/slintlu`。Arch 套件、原始碼安裝程式與 CI 都會這麼做；Nix 套件則建置同一個
  版本並連結到相同位置。

## 啟動殼層

合成器會先在執行檔旁（`../share/lunadash/shell`）尋找殼層，再找建置時的殼層 checkout 與 XDG
資料目錄。目錄中有 `shell.slint` 時，會執行 `slintlu --config <目錄>`，並優先使用安裝的
`libexec/lunadash/slintlu`，其次是 `PATH`、`~/.cargo/bin` 與 `~/.local/bin` 中的 `slintlu`。
session.json 選擇 LunaDash 殼層而找不到 SlintLu 時，`lunadash-session` 會拒絕啟動工作階段（`lunadash-compositor --session-shell` 會印出目前的選擇）。

`LUDASH_SHELL_RENDERER`（`auto`、`opengl`、`vulkan`、`software`）會轉為 `SLINTLU_RENDERER`；
`auto` 則交由 SlintLu 自行選擇並自動備援。詳見[殼層渲染](SHELL_RENDERING.md)。

含有 `shell.qml` 的舊版 checkout 仍會照舊以 Quickshell 啟動。

## 選擇殼層

和 Hyprland 的 `exec-once` 類似，工作階段可以執行 LunaDash 殼層、其他殼層，或不執行殼層。
由 `~/.config/LunaDash/session.json` 決定；沒有這個檔案時使用第一個
`$XDG_CONFIG_DIRS/LunaDash/session.json`（通常是 `/etc/xdg`）；兩者都沒有時啟動 LunaDash 殼層。

```json
{
  "shell": "custom",
  "shellCommand": ["waybar"],
  "launcherCommand": ["fuzzel"],
  "autostart": [["mako"], ["swaybg", "-i", "/home/me/Pictures/wall.png"]]
}
```

- `shell`：`lunadash`（預設）、`custom`（執行 `shellCommand`）或 `none`。
- `launcherCommand`：沒有執行 LunaDash 殼層時，Super 鍵與啟動器快捷鍵要啟動的程式。其他殼層快捷鍵
  （控制中心、剪貼簿、電源選單）此時不會有動作。
- `autostart`：最多 32 個指令，不論使用哪個殼層，都會在工作階段開始時各啟動一次。
- 指令是參數陣列，不經過 command shell 執行；需要管線或變數時請用 `["sh", "-c", "..."]`。
- Welcome 程式與設定精靈只在使用 LunaDash 殼層時開啟。
- `LUNADASH_SHELL=none` 或 `LUNADASH_SHELL=lunadash` 可在單次工作階段覆寫設定檔。

檔案中有未知 key、型別錯誤，或 custom 殼層沒有 `shellCommand` 時，整個檔案都會被忽略：工作階段改用
LunaDash 殼層並在 log 中說明原因。`lunadashctl status` 的 `session` 欄位會顯示目前的設定與錯誤。

安裝程式會寫入這個檔案：`install.sh --shell custom --shell-command waybar` 或
`scripts/install-session.sh --shell none`，引導安裝時兩者也都會詢問。安裝程式只會替換 `shell`
（以及 `shellCommand`），其他 key 會保留。選擇 `custom` 或 `none` 時，`install-session.sh` 會以
`-DLUDASH_INSTALL_SHELL=OFF` 建置：不建置也不安裝殼層檔案與 SlintLu，且
`/etc/xdg/LunaDash/session.json` 預設為 `none`。Arch 套件一律包含 LunaDash 殼層。

## 殼層從 LunaDash 取得的內容

SlintLu 透過控制 socket 辨識 LunaDash 並輪詢其狀態：

- 鍵盤快捷鍵會以請求的形式送到殼層：啟動器（包含單按 Meta 切換）、設定、桌布選擇器、控制中心、
  剪貼簿與電源選單。
- 桌布由合成器管理：殼層顯示 `wallpaperImage`，在殼層中選擇桌布時會送出 `wallpaper-image`、
  `wallpaper-default` 或 `wallpaper-random`。
- 介面語言跟隨 LunaDash 的桌面語言，殼層的語言頁面也會變更整個桌面的語言（`language`）。
- 啟動器會回報是否開啟（`launcher-visible`），讓 Meta 鍵能正確切換。
- 預設應用程式透過 `launch-default` 開啟（終端機、檔案、瀏覽器）。
- 在 LunaDash 上殼層不顯示自己的初次使用卡片。設定完成前，合成器會在工作階段開始時開啟 Welcome
  程式（`lunadashctl setup` 後也會再開一次）；按「Start desktop」或 Enter 即完成設定。
- 殼層介面透過 `wp_cursor_shape_v1` 設定滑鼠游標，並透過 `wp_fractional_scale_v1` 以分數縮放繪製。
  只要指標下方的介面沒有自己的游標（包括 `slurp` 選取畫面這類介面剛關閉時），合成器都會顯示預設箭頭。

殼層呼叫了合成器沒有處理的方法時，`scripts/ci/check_shell_actions.py` 會失敗。
`tests/wayland/test_session_shell.py` 會以固定版本的殼層與 SlintLu 啟動合成器，並檢查上述往返
流程（`scripts/run-tests.sh` 會執行這兩項）。

## Slint 殼層尚未提供的功能

舊的 Quickshell 殼層有些功能尚未移植：

- 殼層模組（`data/modules`，見[模組](MODULES.md)）與 Quickshell 外掛，包括桌面小工具
  （見[外掛](PLUGINS.md)）；
- 由合成器驅動的設定頁面（顯示器、快捷鍵、輸入、預設應用程式、外掛、更新）。殼層自己的設定涵蓋
  外觀、桌布、面板與 Dock、通知、電源與鎖定、語言與關於；
- 工作階段鎖定與閒置鎖定需要 `ext-session-lock-v1` 與 `ext-idle-notify-v1`，合成器目前尚未提供，
  因此 SlintLu 會停用鎖定畫面。

上述文件仍描述 Quickshell 的實作。
