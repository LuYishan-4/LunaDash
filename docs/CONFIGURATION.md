# First-run setup and configuration

The first launch presents four steps: language, network, appearance and a short desktop introduction. Continue offline if needed. Changes to language and appearance are saved as they are made; **Start desktop** marks the guide complete. Desktop settings can reopen the guide at any time.

## Network

LuDash reads the existing system connection state through NetworkManager's D-Bus API every five seconds. It does not create accounts, modify connection profiles, enable services or send connectivity probes. A connected interface is not automatically labeled as Internet access: captive portals and unknown connectivity have separate labels. Without NetworkManager, interface detection reports only a possible link, with Internet access unverified.

**Configure network** opens `nm-connection-editor`. If it is unavailable, LuDash tries `nmtui` in foot, Konsole or Alacritty. The first-run guide temporarily yields to the editor and returns after its window closes. Passwords remain in the external editor; LuDash never stores them or sends them over its IPC. Changing system connections may require the distribution's working polkit authentication agent. LuDash does not currently provide one.

Arch optional packages: `networkmanager nm-connection-editor`. Do not enable NetworkManager alongside a conflicting manager. Existing host connections need no changes. A native Wi-Fi scanner, credential form and captive-portal browser flow are not implemented.

## Saved preferences

With normal XDG settings, the compositor stores configuration in `~/.config/LuDash/LuDash.conf`. `XDG_CONFIG_HOME` changes its root. Keep this file private if it includes local wallpaper paths. The built-in tools and compositor share the LuDash organization/application identity.

| Preference | Default | Accepted values |
| --- | --- | --- |
| `desktop/accent` | `#7dcccf` | `#RRGGBB` |
| `desktop/gap` | 12 | Integer, 4–32 pixels |
| `desktop/panelHeight` | 28 | Integer, 24–40 pixels |
| `desktop/overview` | true | Boolean |
| `desktop/showHostDetails` | false | Boolean |
| `appearance/language` | System locale | `en_US`, `zh_TW` |
| `appearance/wallpaperMode` | image | image or shader |
| `appearance/wallpaperImage` | Bundled image | Validated local image path |
| `appearance/wallpaper` | 0 | Shader palette 0 or 1 |
| `session/setupComplete` | false | Boolean |

Use the UI or IPC for live changes. Manual file edits are intended for a stopped session; an active session can overwrite them. Invalid saved appearance values fall back to defaults. `LUDASH_LANGUAGE` takes precedence over the saved language. `LUDASH_SKIP_SETUP=1` bypasses the guide for automated testing without completing the saved guide.

For a session launched with `--socket ludash-test`, open another terminal:

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/ludashctl status
./build/ludashctl appearance '{"accent":"#c4b5fd","gap":20,"panelHeight":32}'
./build/ludashctl appearance '{"overview":true,"showHostDetails":false}'
./build/ludashctl wallpaper-image /absolute/path/wallpaper.png
./build/ludashctl wallpaper-default
./build/ludashctl language en_US
./build/ludashctl setup
```

Malformed JSON, unknown keys, wrong types and invalid ranges are rejected before any preference is changed. `finish-setup` completes the guide; `configure-network` opens the available editor. Shell state updates within about 700 ms. Native application colors are currently fixed; accent customization applies to the shell and compositor frame borders.

For deeper customization, edit the feature QML under `qml/` and restart. This is trusted local code, not a sandboxed theme package. Native effect plugins are separately opt-in; see [Plugins](PLUGINS.md).

Reference: [NetworkManager D-Bus API](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.html).
