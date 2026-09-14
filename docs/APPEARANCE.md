# Appearance

The default desktop takes visual inspiration from Caelestia: rounded control surfaces, a soft blue accent, pill-shaped controls, a tabbed dashboard and consistent transitions. The bundled florist wallpaper remains available. Quickshell draws the shell; C++ owns tiling and composition.

- The left panel switches workspaces and provides session controls.
- The center uses `BrandIcon`, which aspect-fills and crops `data/assets/icon.png`. Clicking it opens the launcher with a downward reveal anchored at the panel center.
- The dashboard has working overview, performance and workspace pages using real system/window state.
- The right panel hosts visible StatusNotifier items through Quickshell `SystemTray` and `IconImage`, followed by compact clock, network, CPU and battery status. Fcitx, Discord and Docker icons appear when those applications export an SNI item.
- The launcher presents one unified list: built-in Settings, Files, Terminal and Monitor entries plus installed Quickshell `DesktopEntries`. Every row has an icon, name and description. Search tokenizes the query and ranks metadata matches; there are no separate categories or open-window section.
- User/hostname display is off by default. The dashboard starts closed and can be toggled or kept open through saved preferences.

An animated LunaDah logo covers shell startup and then fades away; reduced-motion settings shorten or disable its movement. The first-run guide appears only until completed. Demo applications open only with `--demo`.

Settings provide four accent presets, 4–32 px gaps, 32–56 px panel height, card visibility, identity display, language and wallpaper. These preferences persist across sessions. The CLI also accepts any six-digit hexadecimal accent color; see [Configuration](CONFIGURATION.md).

Choose the bundled AI-generated florist image, a local PNG/JPEG/WebP, or the Dusk/Forest shader. Images must be readable local files up to 64 MiB and 32 million pixels. URLs are not downloaded. A missing configured image falls back to the bundled image. Images fill the output with aspect-preserving cropping.

Application windows have configurable backdrop blur and opacity by default; see [Effects](EFFECTS.md). Shell controls use alpha blending and animated reveals. Application shortcuts are functional entry points. StatusNotifier tray hosting is implemented, but notification hosting and audio controls are not. Network status and connection settings are available in the setup guide and settings.

```sh
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
# build/desktop-preview.png and build/desktop-state.json
```

Theme defaults live in `qml/style/Theme.qml`; runtime preferences override accent and panel height. The packaged BrandIcon asset is installed at `/usr/share/lunadah/data/assets/icon.png`. QML customization requires restarting the shell/session. C++ system data and wallpaper handling live in `system_status` and `wallpaper`. Traditional Chinese text remains in `data/translations/zh_TW.json`.

Design reference: [Caelestia shell](https://github.com/caelestia-dots/shell), GPL-3.0. LunaDah uses an original implementation inspired by its rounded surfaces and motion; it does not embed Caelestia or inherit its Hyprland-specific services.
