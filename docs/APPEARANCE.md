# Appearance

The default desktop combines Caelestia-inspired rounded surfaces and motion with one compact, niri-inspired top `TopPanel`. The bundled florist wallpaper remains available. Quickshell draws the shell; C++ owns tiling and composition.

- The left side switches workspaces, opens session controls and embeds grouped-application cells inline. Each cell contains its members' application icons, including dimmed minimized members. Clicking an exact member restores, focuses and scrolls it into view; dragging it onto a member in another column groups it there. Right-click or the minus badge expels that member into its own column.
- The former standalone `ColumnStrip` is no longer instantiated as a second Quickshell layer. `TopPanel` reuses its grouped-cell/member behavior inline next to the workspace/session controls.
- The center is a three-part overview / accent launcher / settings selector. Its accent-colored launcher carries the Lambda (`Λ`) glyph and opens the unified launcher with a downward reveal anchored at panel center; the adjacent controls toggle overview and settings.
- The dashboard has working overview, performance and workspace pages using real system/window state.
- The right side hosts visible StatusNotifier items through Quickshell `SystemTray` and `IconImage`, followed by compact clock, network and battery status. Identity-based fallbacks resolve Fcitx, Discord and Docker tray icons when their supplied icon is missing or unusable.
- The launcher presents one unified list: built-in Settings, Files, Terminal and Monitor entries plus installed Quickshell `DesktopEntries`. Every row has an icon, name and description. Search tokenizes the query and ranks metadata matches; there are no separate categories or open-window section.
- User/hostname display is off by default. The dashboard starts closed and can be toggled or kept open through saved preferences.

An animated LunaDah logo covers shell startup and then fades away; reduced-motion settings shorten or disable its movement. The first-run guide appears only until completed. Demo applications open only with `--demo`.

Settings provide four accent presets, 4–32 px gaps, 32–56 px panel height, card visibility, identity display, language and wallpaper. These preferences persist across sessions. The CLI also accepts any six-digit hexadecimal accent color; see [Configuration](CONFIGURATION.md).

Choose the bundled AI-generated florist image, a local PNG/JPEG/WebP, or the Dusk/Forest shader. The local-image button opens LunaDah's own QWidget image picker, not `QFileDialog`. It provides typed directory/up navigation, list and grid views, file metadata and a bounded aspect-preserving preview. Images must be readable local files up to 64 MiB and 32 megapixels. URLs are not downloaded. A missing configured image falls back to the bundled image. Images fill the output with aspect-preserving cropping.

Application windows have configurable backdrop blur and opacity by default. Tiled and generic windows have no frame move/drag handler and cannot be repositioned with the pointer; pointer dragging is reserved for application icons in the top panel to group columns.  see [Effects](EFFECTS.md). Tiled columns hold at most four total members, including minimized windows, and divide their vertical work area equally among visible members. The compositor places windows in `workArea`; for the top panel its y origin is below the reported `panelExtent`, matching the panel's layer-shell exclusive area, before applying the configured window gap. Kitty starts maximized to that work area; generic windows, dialogs and the image picker retain normal initial placement. `Super+F` toggles maximize, while the frame's top-right controls minimize or close quickly.

Settings is a fullscreen Quickshell/QML overlay with exclusive keyboard focus, a small outer margin and a quick-hide × control. Its top margin begins below `Theme.barHeight`, so the settings surface does not cover the top panel. Shell controls use alpha blending and animated reveals. Application shortcuts are functional entry points. StatusNotifier tray hosting is implemented, but notification hosting is not. The Fcitx tray icon is fixed; candidate popup behavior remains dependent on the incomplete compositor input-method bridge. Network status and connection settings are available in the setup guide and settings.

```sh
LUDASH_TEST_OVERVIEW=1 ./scripts/test-wayland.sh
# build/desktop-preview.png and build/desktop-state.json
```

Theme defaults live in `qml/style/Theme.qml`; runtime preferences override accent and panel height. The generated cropped icon is `data/assets/lunadah.png`; `LUNADAH_ASSET_DIR` gives Quickshell an absolute filesystem path so packaged QML does not resolve the image to `qrc:/qs-blackhole`. Known LunaDah applications use bundled vector icons and unknown applications use a themed initial badge when the host icon theme lacks a requested name.  the shell reads its installed copy under `/usr/share/lunadah/data/assets/`, and packaging also installs `/usr/share/icons/hicolor/512x512/apps/lunadah.png` for the `Icon=lunadah` desktop entry. The compositor exports a resolved `icon` for each client and grouped member: Files uses `system-file-manager`, Settings uses `preferences-system`, Kitty uses `kitty`, and Monitor uses `utilities-system-monitor`. QML customization requires restarting the shell/session. C++ system data and wallpaper handling live in `system_status` and `wallpaper`. Traditional Chinese text remains in `data/translations/zh_TW.json`.

Design reference: [Caelestia shell](https://github.com/caelestia-dots/shell), GPL-3.0. LunaDah uses an original implementation inspired by its rounded surfaces and motion; it does not embed Caelestia or inherit its Hyprland-specific services.
