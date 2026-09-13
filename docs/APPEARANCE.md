# Appearance

The default desktop uses a thin segmented top panel, dark green and teal colors, a translucent information card, narrow window borders and a full-screen florist wallpaper. Quickshell draws the shell; C++ owns tiling and composition.

- The left diamond opens the launcher, workspace dots select desktops 1–4, and `~` opens the console.
- The center provides files, notes, the clock and CPU history. Click LuDash or the clock to toggle the information card.
- The right shows real CPU and memory usage, and battery capacity when available. The gear opens settings; the power symbol opens logout confirmation.
- The launcher's open-window list restores minimized windows and focuses windows across workspaces.
- The lower-left card uses local OS, kernel, CPU, RAM and home-filesystem statistics. User and hostname display is off by default and can be enabled.

The first-run guide appears only until completed. Demo applications open only with `--demo`.

Settings provide four accent presets, 4–32 px gaps, 24–40 px panel height, card visibility, identity display, language and wallpaper. These preferences persist across sessions. The CLI also accepts any six-digit hexadecimal accent color; see [Configuration](CONFIGURATION.md).

Choose the bundled AI-generated florist image, a local PNG/JPEG/WebP, or the Dusk/Forest shader. Images must be readable local files up to 64 MiB and 32 million pixels. URLs are not downloaded. A missing configured image falls back to the bundled image. Images fill the output with aspect-preserving cropping.

Transparency uses alpha blending; background blur is not implemented. Application shortcuts are functional entry points. A complete system tray and audio controls are not yet implemented. Network status and connection settings are available in the setup guide and settings.

```sh
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
# build/desktop-preview.png and build/desktop-state.json
```

Theme defaults live in `qml/style/Theme.qml`; runtime preferences override accent and panel height. QML customization requires restarting the shell/session. C++ system data and wallpaper handling live in `system_status` and `wallpaper`. Traditional Chinese text remains in `data/translations/zh_TW.json`.
