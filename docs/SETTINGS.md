# Settings center

Open the panel gear, right-click the wallpaper, choose Settings in the launcher, or run `ludash-desktop --app settings` inside LuDash. All entries lead to the same Quickshell settings center. The sidebar uses consistent outline icons and searches translated category names. Selected pages sit in a separate content card; switches, sliders, spacing and color swatches share the shell theme. Changes to LuDash preferences are saved automatically; destructive system operations remain in the external tools that perform them.

## Coverage

| Page | Direct LuDash controls | System or host integration / limits |
| --- | --- | --- |
| General | Language, shell font, 12/24-hour clock, first-run guide, confirmed preference reset | Font choice affects the shell; external application themes remain independent |
| Appearance | Wallpaper image/palettes, accent, gaps, panel height, dashboard visibility, smooth blur, opacity and animation duration | Blur applies to application frames; no KDE blur protocol |
| Windows and workspaces | 1–9 workspaces, saved master width, window gaps, default floating mode | Reducing the count moves windows to a remaining workspace; no arbitrary shortcut editor yet |
| Shell modules | JSON styles, templates, code trust and built-in recovery | [Module schema and contract](MODULES.md); custom QML is not sandboxed |
| Display | Current output information and three nested window sizes | Physical modes, scale, rotation and refresh are managed by the host; host monitor settings can be opened when available. Standalone multi-monitor, HDR and night light remain unavailable |
| Keyboard and pointer | Seven keyboard layouts, repeat rate/delay, cursor size for the next session, input test field | Input-method editor and host mouse/touchpad settings; standalone libinput device configuration remains unavailable |
| Sound | Default output and microphone volume/mute through WirePlumber | Pavucontrol/pwvucontrol handles routing and devices; missing services disable direct controls |
| Network | Current connection state | NetworkManager editor handles Wi-Fi, Ethernet, VPN and saved profiles; LuDash does not store network passwords |
| Bluetooth | Tool availability and package guidance | Blueman handles pairing and adapters |
| Power and battery | Reported battery charge and supported power profiles | Host power tool handles lid/idle policy. No standalone suspend policy or backlight controls yet |
| Applications and startup | Built-in startup selection, package/plugin/X11 launchers | Default-application editor when installed; arbitrary desktop-entry autostart/session restore is not implemented |
| Privacy and accessibility | Host-identity visibility, reduced motion, native-plugin access | Optional host accessibility/locking settings. LuDash has no secure lock screen, notification service, screen reader integration or portal permission UI yet |
| Users, date and time | Settings-tool discovery | Installed account/time editors handle authorization; LuDash does not create users or retain passwords |
| Printers and storage | Settings-tool discovery | system-config-printer and GNOME Disks; their confirmation flows govern destructive actions |
| About | Version, OS, kernel, architecture and actual graphics API | Development status and documentation entry points |

An installed executable means the editor can be launched, not that every system service or authorization agent is present. KDE module providers are offered only when their plugin file exists. Host-only tools use the original host environment and are not advertised as native LuDash hardware controls. They are unavailable in standalone EGLFS sessions.

The internal audio limit is 100%. LuDash reads service state periodically rather than pretending a requested change succeeded. Helper processes have a 2.5-second timeout and a 64 KiB output cap. Audio requests accept only the default input/output selector and bounded volume or boolean mute; power profiles must appear in the service's supported list. System tool commands come from a fixed allowlist and are never evaluated by a shell.

## Saved preferences

Additional keys in the existing `[desktop]` group:

| Key | Default | Accepted values |
| --- | --- | --- |
| `workspaceCount` | 4 | Integer 1–9 |
| `masterRatio` | 56 | Integer 30–70, percent |
| `defaultFloating` | false | Boolean |
| `keyboardLayout` | us | us, gb, de, fr, es, jp, tw |
| `keyRepeatRate` | 25 | Integer 0–60; zero disables repeat |
| `keyRepeatDelay` | 600 | Integer 200–1500 ms |
| `cursorSize` | 24 | Integer 16–64 px; next session |
| `fontFamily` | sans-serif | sans-serif, serif, monospace |
| `clock24Hour` | true | Boolean |
| `startupApps` | empty | Unique built-in IDs: files, console, monitor, welcome |

Startup selection runs after the shell starts in sessions whose first-run guide has been completed. Network, audio, power and account settings belong to their system services and are not copied into LuDash's INI file. Resetting desktop preferences does not delete documents, reset language/wallpaper selections or modify system-service configuration. Nested window size is session-local.

## IPC and tests

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/ludashctl open-settings appearance
./build/ludashctl appearance '{"workspaceCount":6,"masterRatio":60}'
./build/ludashctl appearance '{"keyboardLayout":"us","keyRepeatRate":25,"keyRepeatDelay":600}'
./build/ludashctl appearance '{"startupApps":["files"]}'
./build/ludashctl desktop-size 1280x720
```

`audio`, `power-profile` and `system-tool` are explicit user actions. Integration tests load every page and reject invalid service commands; they do not alter the host volume, microphone, network, power profile, users or storage. Unit tests cover command validation and helper limits. The old Notes source, entry points and tests are removed; existing user text files are left untouched. IME testing can use the settings input field or Console.

References: [WirePlumber wpctl](https://pipewire.pages.freedesktop.org/wireplumber/man/wpctl.html) and the installed system tools' own help/documentation.

Default terminal and file-manager argument arrays are edited under Applications and startup. Empty arrays select Konsole/Fish and LuDash Files. See [Default apps and Files](DEFAULT_APPS_AND_FILES.md).
