# Languages and input methods

C++ and QML use English strings as translation keys. External language catalogs under `data/translations/` provide English, Traditional Chinese, Simplified Chinese and Japanese. Quickshell receives the selected dictionary through IPC; native tools apply the language when reopened. `LUDASH_LANGUAGE` overrides the saved choice for testing. Standard Qt dialogs also use the distribution's translations package.

The compositor advertises wlroots text-input-v3, input-method-v2 and virtual-keyboard-v1 globals. `src/compositor/input/Input.cpp` implements focus transfer, preedit/commit forwarding, input-method keyboard grabs and popup positioning; `Keyboard.cpp` applies xkbcommon maps and repeat settings to the wlroots keyboard. This no longer depends on optional Qt Wayland Compositor private headers.

Fcitx5 remains a separate process. The session uses toolkit module variables (`QT_IM_MODULE`, `QT_IM_MODULES`, `GTK_IM_MODULE`, `SDL_IM_MODULE` and `XMODIFIERS`) and can launch an installed Fcitx5 after its own Wayland socket exists. Its environment is scoped to the LunaDash session. The Quickshell system tray displays StatusNotifier items and falls back to bundled glyphs when icons cannot be decoded.

The protocol-global CI test establishes that the interfaces are announced. It does not verify every input method or toolkit. In a real session test switching input methods, preedit, candidate selection, commit/deletion, cursor movement, focus transitions and popup placement in Qt, GTK, Chrome/Electron and Zed. Physical keyboard layout, NumLock/CapsLock, AltGr and device hotplug also require physical-session validation.

Optional Fcitx packages on Arch include `fcitx5`, `fcitx5-qt`, `fcitx5-configtool` and the desired language add-ons. See [Fcitx5's Wayland guide](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en) for toolkit setup and [testing](TESTING_AND_FILES.md) for LunaDash's verification scope.

## NyxNiri-inspired desktop, Orbit and live wallpapers

See [the desktop integration guide](NYXNIRI_DESKTOP.md) for the new modules, palette and portal services, shortcuts, dependencies, and current verification limits.
