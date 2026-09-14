# Appearance

The default desktop takes visual inspiration from Caelestia: rounded control surfaces, a soft blue accent, pill-shaped controls, a tabbed dashboard and consistent transitions. The bundled florist wallpaper remains available. Quickshell draws the shell; C++ owns tiling and composition.

- The left panel switches workspaces and provides session controls.
- The center uses `BrandIcon`, which aspect-fills and crops `data/assets/icon.png`. Clicking it opens the launcher with a downward reveal anchored at the panel center.
- The dashboard has working overview, performance and workspace pages using real system/window state.
- The right panel hosts visible StatusNotifier items through Quickshell `SystemTray` and `IconImage`, followed by compact clock, network, CPU and battery status. Fcitx, Discord and Docker icons appear when those applications export an SNI item.
- The launcher presents one unified list: built-in Settings, Files, Terminal and Monitor entries plus installed Quickshell `DesktopEntries`. Every row has an icon, name and description. Search tokenizes the query and ranks metadata matches; there are no separate categories or open-window section.
- The niri-inspired top-left `ColumnStrip` appears only while tiled columns exist and follows the live theme accent. Each column has one cell containing its members' application icons, including dimmed minimized members. Clicking an exact member restores, focuses and scrolls it into view; dragging it onto a member in another column groups it there. Right-click or the minus badge expels a grouped member into its own column.
- User/hostname display is off by default. The dashboard starts closed and can be toggled or kept open through saved preferences.

An animated LunaDah logo covers shell startup and then fades away; reduced-motion settings shorten or disable its movement. The first-run guide appears only until completed. Demo applications open only with `--demo`.

Settings provide four accent presets, 4–32 px gaps, 32–56 px panel height, card visibility, identity display, language and wallpaper. These preferences persist across sessions. The CLI also accepts any six-digit hexadecimal accent color; see [Configuration](CONFIGURATION.md).

Choose the bundled AI-generated florist image, a local PNG/JPEG/WebP, or the Dusk/Forest shader. The local-image button opens LunaDah's own QWidget image picker, not `QFileDialog`. It provides typed directory/up navigation, list and grid views, file metadata and a bounded aspect-preserving preview. Images must be readable local files up to 64 MiB and 32 megapixels. URLs are not downloaded. A missing configured image falls back to the bundled image. Images fill the output with aspect-preserving cropping.

Application windows have configurable backdrop blur and opacity by default; see [Effects](EFFECTS.md). Tiled columns hold at most four total members, including minimized windows, and divide their vertical work area equally among visible members. Kitty starts maximized to the full work area; generic windows, dialogs and the image picker retain normal initial placement. `Super+F` toggles maximize, while the frame's top-right controls minimize or close quickly.

Settings is a fullscreen Quickshell/QML overlay with exclusive keyboard focus, a small outer margin and a quick-hide × control. Shell controls use alpha blending and animated reveals. Application shortcuts are functional entry points. StatusNotifier tray hosting is implemented, but notification hosting is not. Network status and connection settings are available in the setup guide and settings.

```sh
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
# build/desktop-preview.png and build/desktop-state.json
```

Theme defaults live in `qml/style/Theme.qml`; runtime preferences override accent and panel height. The packaged BrandIcon asset is installed at `/usr/share/lunadah/data/assets/icon.png`. QML customization requires restarting the shell/session. C++ system data and wallpaper handling live in `system_status` and `wallpaper`. Traditional Chinese text remains in `data/translations/zh_TW.json`.

Design reference: [Caelestia shell](https://github.com/caelestia-dots/shell), GPL-3.0. LunaDah uses an original implementation inspired by its rounded surfaces and motion; it does not embed Caelestia or inherit its Hyprland-specific services.
