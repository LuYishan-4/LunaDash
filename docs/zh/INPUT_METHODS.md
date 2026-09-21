# 語言與輸入法

[English](../en/INPUT_METHODS.md) · [繁中索引](README.md)

C++ 與 QML 使用英文 source string 作 translation key。外部 catalog 位於 `data/translations/`，目前有 English、繁中、簡中、日文。Quickshell 透過 IPC 取得選定 dictionary；原生工具重新開啟後套用語言。`LUDASH_LANGUAGE` 可供測試覆蓋 saved language。

Compositor 公布：

- text-input-v3
- input-method-v2
- virtual-keyboard-v1

`src/compositor/input/Input.cpp` 處理 focus transfer、preedit/commit forwarding、input-method keyboard grab 與 popup position；`Keyboard.cpp` 透過 xkbcommon 套 keyboard map 與 repeat 設定。這已不依賴 Qt Wayland Compositor private headers。

Fcitx5 仍是獨立 process。LunaDash session 設定 `QT_IM_MODULE`、`QT_IM_MODULES`、`GTK_IM_MODULE`、`SDL_IM_MODULE`、`XMODIFIERS`，並在自己的 Wayland socket 建立後才啟動可用的 Fcitx5。這些變數只作用於 LunaDash session，不改 host desktop。

Protocol CI 只能證明 global 有 advertised，不能證明所有 toolkit/input method 都正常。真實 session 應測：

- input method 切換
- preedit/candidate/commit/delete
- cursor movement
- focus transition
- candidate popup position
- Qt / GTK / Chrome/Electron / Zed
- physical layout、Caps/Num lock、AltGr、hotplug

Arch 常用套件：`fcitx5`、`fcitx5-qt`、`fcitx5-configtool` 與對應語言 addon。
