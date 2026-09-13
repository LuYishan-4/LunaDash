# Appearance

The default desktop takes visual inspiration from Caelestia: rounded control surfaces, a soft blue accent, pill-shaped controls, a tabbed dashboard and consistent transitions. The bundled florist wallpaper remains available. Quickshell draws the shell; C++ owns tiling and composition.

- The left star opens the launcher; numbered pills switch workspaces and the console button opens a native tool.
- The center shows the focused application. The diamond and clock open the dashboard.
- The dashboard has working overview, performance and workspace pages using real system/window state.
- The right shows a clock, network link status, CPU usage when space allows, optional battery state, settings and logout.
- The launcher searches applications, restores minimized windows and provides an X11 application entry point.
- User/hostname display is off by default. The dashboard starts closed and can be toggled or kept open through saved preferences.

The first-run guide appears only until completed. Demo applications open only with `--demo`.

Settings provide four accent presets, 4–32 px gaps, 32–56 px panel height, card visibility, identity display, language and wallpaper. These preferences persist across sessions. The CLI also accepts any six-digit hexadecimal accent color; see [Configuration](CONFIGURATION.md).

Choose the bundled AI-generated florist image, a local PNG/JPEG/WebP, or the Dusk/Forest shader. Images must be readable local files up to 64 MiB and 32 million pixels. URLs are not downloaded. A missing configured image falls back to the bundled image. Images fill the output with aspect-preserving cropping.

Application windows have configurable backdrop blur and opacity by default; see [Effects](EFFECTS.md). Shell controls use alpha blending and animated reveals. Application shortcuts are functional entry points. A complete system tray and audio controls are not yet implemented. Network status and connection settings are available in the setup guide and settings.

```sh
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
# build/desktop-preview.png and build/desktop-state.json
```

Theme defaults live in `qml/style/Theme.qml`; runtime preferences override accent and panel height. QML customization requires restarting the shell/session. C++ system data and wallpaper handling live in `system_status` and `wallpaper`. Traditional Chinese text remains in `data/translations/zh_TW.json`.

Design reference: [Caelestia shell](https://github.com/caelestia-dots/shell), GPL-3.0. LuDash uses an original implementation inspired by its rounded surfaces and motion; it does not embed Caelestia or inherit its Hyprland-specific services.
