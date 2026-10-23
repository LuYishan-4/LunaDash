# First-run setup and configuration

The first-login Quickshell guide offers language and reduced-motion controls and links to https://luyishan-4.github.io/LunaDash/. The standalone `lunadash-desktop --app welcome` tool uses a separate Slint Wayland window with the same control-socket actions. **Start desktop** saves setup completion, including when offline; opening the documentation link alone does not complete setup. General settings can reopen the guide. Network preferences remain in Settings.

## Network

LunaDash reads NetworkManager device, saved-connection and Wi-Fi details with `nmcli` every 30 seconds and refreshes after LunaDash network actions. If `nmcli` is unavailable, it reads NetworkManager's D-Bus state; without NetworkManager, interface detection reports only a possible link. LunaDash does not create accounts, modify connection profiles unless requested in Settings, enable services or send connectivity probes. A connected interface is not automatically labeled as Internet access: captive portals and unknown connectivity have separate labels.

**Configure network** opens `nm-connection-editor`. If it is unavailable, LunaDash tries `nmtui` in Kitty, Konsole, Alacritty or foot. Passwords remain in the external editor; LunaDash never stores them or sends them over its IPC. Changing system connections may require the distribution's working polkit authentication agent. LunaDash does not currently provide one.

Arch optional packages: `networkmanager nm-connection-editor`. Do not enable NetworkManager alongside a conflicting manager. Existing host connections need no changes. A native Wi-Fi scanner, credential form and captive-portal browser flow are not implemented.

## Saved preferences

Desktop preferences, wallpaper state, and shortcut bindings are stored atomically in `${XDG_CONFIG_HOME:-$HOME/.config}/LunaDash/desktop-preferences.json`, with owner-only file permissions. On first use of each settings group, LunaDash imports valid `desktop/*` values, setup completion, wallpaper selection/shader/history/favorites/revision, and shortcut bindings from the older `LunaDash.conf` or `LunaDash.ini`. It leaves the old file in place. Other appearance settings such as language remain there until their storage is migrated.

| Preference | Default | Accepted values |
| --- | --- | --- |
| `desktop/accent` | `#9ccbfb` | `#RRGGBB` |
| `desktop/gap` | 12 | Integer, 4–32 pixels |
| `desktop/panelHeight` | 40 | Integer, 32–56 pixels |
| `desktop/overview` | false | Boolean |
| `desktop/blur` | true | Boolean |
| `desktop/blurRadius` | 18 | Integer, 0–32 |
| `desktop/windowOpacity` | 96 | Integer, 60–100 percent |
| `desktop/animations` | true | Boolean |
| `desktop/animationDuration` | 220 | Integer, 0–600 milliseconds |
| `desktop/showHostDetails` | false | Boolean |
| `appearance/language` | System locale | `en_US`, `zh_TW`, `zh_CN`, `ja_JP` |
| `appearance/wallpaperMode` | image | image or shader |
| `appearance/wallpaperImage` | Bundled image | Validated local image path |
| `appearance/wallpaper` | 0 | Shader palette 0 or 1 |
| `session/setupComplete` | false | Boolean |

Use the UI or IPC for live changes. Manual file edits are intended for a stopped session; an active session can overwrite them. Invalid saved appearance values fall back to defaults. `LUDASH_LANGUAGE` takes precedence over the saved language. `LUDASH_SKIP_SETUP=1` bypasses the guide for automated testing without completing the saved guide.

For a session launched with `--socket ludash-test`, open another terminal:

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl status
./build/lunadashctl appearance '{"accent":"#c4b5fd","gap":20,"panelHeight":32}'
./build/lunadashctl appearance '{"overview":true,"showHostDetails":false}'
./build/lunadashctl choose-wallpaper
./build/lunadashctl wallpaper-image /absolute/path/wallpaper.png
./build/lunadashctl wallpaper-default
./build/lunadashctl language en_US
./build/lunadashctl setup
```

Malformed JSON, unknown keys, wrong types and invalid ranges are rejected before any preference is changed. `finish-setup` completes the guide; `configure-network` opens the available editor. `choose-wallpaper` opens the in-shell picker inside the settings surface; a confirmed PNG/JPEG/WebP path returns through `lunadashctl wallpaper-image`, while cancellation makes no change. The picker uses list/grid directory navigation, a bounded preview and a 64 MiB per-file limit. Shell state updates within about 700 ms. Native application colors are currently fixed; accent customization applies to the single `TopPanel`, its inline grouped-application cells and compositor frame borders.

For deeper customization, edit the feature QML under `LunaDash-Shell/` and restart. This is trusted local code, not a sandboxed theme package. Native effect plugins are separately opt-in; see [Plugins](PLUGINS.md).

Reference: [NetworkManager D-Bus API](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.html).

See [Effects](EFFECTS.md) for blur/reduced-motion behavior and [XWayland](XWAYLAND.md) for optional legacy application support. Old saved panel heights outside the supported range fall back to the new default.

The dedicated settings center and its additional workspace, keyboard, font, clock and startup keys are documented in [Settings](SETTINGS.md).
