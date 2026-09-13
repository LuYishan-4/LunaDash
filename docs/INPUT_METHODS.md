# Languages and input methods

C++ and QML use English source strings as translation keys. Traditional Chinese mappings live only in `data/translations/zh_TW.json`. Quickshell receives the selected dictionary through local IPC and updates live. Native tools apply language changes when reopened. `LUDASH_LANGUAGE=en_US` or `zh_TW` overrides the saved language for testing.

Currently only English and Traditional Chinese are registered. Adding a language requires a JSON dictionary, CMake resource registration, selection logic and UI options. Do not add translated strings to feature source files. Qt's standard dialogs use the distribution's `qt6-translations` package.

The compositor registers text-input v2/v3 and Qt's input-method protocol. This supports the Qt platform input route; it does not implement a complete Fcitx5 input-method-v2 compositor bridge.

Optional Arch packages:

```sh
sudo pacman -S --needed fcitx5 fcitx5-qt fcitx5-configtool fcitx5-chinese-addons
```

First verify Fcitx5 in the host desktop. Then evaluate:

```sh
QT_QPA_PLATFORM=wayland QT_IM_MODULE=fcitx ./build/ludash-compositor --socket ludash-ime
```

Qt clients launched by LuDash use `QT_IM_MODULE=wayland` by default. To separately evaluate the direct Qt Fcitx module:

```sh
WAYLAND_DISPLAY=ludash-ime QT_QPA_PLATFORM=wayland QT_IM_MODULE=fcitx ./build/ludash-desktop --app console
```

In the console input field (without running the entered text), test switching methods, preedit, candidate selection, commit, deletion, cursor movement, focus changes and candidate-window positioning. The direct-module route does not prove compositor bridge support.

Complete Fcitx5/IBus behavior, candidate placement and GTK/Electron compatibility still require physical-session testing. LuDash does not automatically start or reset input-method daemons. Open input-method tools from Desktop settings when installed.

Reference: [Fcitx5 on Wayland](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en).
