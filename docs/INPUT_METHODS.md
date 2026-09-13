# 多語言與輸入法

C++／QML 來源以英文作為翻譯鍵，沒有中文介面字串。英文直接使用來源，繁中對照放在 `data/translations/zh_TW.json`。Quickshell 透過本機控制介面取得語言包，切換後直接更新；原生工具在下次啟動時套用。測試可用 `LUDASH_LANGUAGE=en_US` 或 `zh_TW` 固定語言。

目前只有英文與繁中。新增語言包需新增 JSON、註冊 CMake resource 及語言選項；不需要把翻譯放進功能原始碼。Qt 標準檔案對話框另外使用系統 `qt6-translations`。

Compositor 註冊 `text-input` v2、v3 與 Qt text-input-method 協定，讓 Qt 的平台輸入法可向 Wayland 用戶端傳送輸入。這不等同實作 Fcitx5 的完整 input-method-v2 compositor bridge。

Arch 可選裝：

```sh
sudo pacman -S --needed fcitx5 fcitx5-qt fcitx5-configtool fcitx5-chinese-addons
```

先在原本的桌面確認 Fcitx5 可正常輸入，再測 LuDash：

```sh
QT_QPA_PLATFORM=wayland QT_IM_MODULE=fcitx ./build/ludash-compositor --socket ludash-ime
```

LuDash 啟動的 Qt 用戶端預設 `QT_IM_MODULE=wayland`，透過 compositor 的 text-input 路徑輸入。若需單獨驗證 Fcitx Qt 模組，可另外執行：

```sh
WAYLAND_DISPLAY=ludash-ime QT_QPA_PLATFORM=wayland QT_IM_MODULE=fcitx ./build/ludash-desktop --app notes
```

在筆記測試：切換輸入法、組字、選字、提交、刪除、移動游標、切換焦點，以及候選字視窗位置。第二條命令是直接 Qt 輸入法模組路徑，不代表 compositor bridge 已通過。

目前完整 Fcitx5／IBus 端到端、候選字定位與 GTK／Electron 相容性仍待實機驗證。IBus 可使用其平台模組測試，LuDash 不自動啟動或重設使用者的輸入法 daemon。

參考：[Fcitx5 Wayland 支援與環境變數](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en)。
