# Settings center

Right-click the wallpaper, use the settings side of the panel's centered three-part selector, choose Settings in the launcher, or run `lunadah-desktop --app settings` inside LunaDah. All entries lead to the same fullscreen Quickshell/QML settings overlay. It takes exclusive keyboard focus, keeps a small theme-controlled margin around the surface, and begins below `Theme.barHeight` so it does not cover the top panel. It closes with Escape from search and provides a top-right quick-hide × control. The sidebar uses consistent outline icons. Search tokenizes the query and indexes translated category names, individual setting names, descriptions and keywords. Ranked setting-level results open the matching page directly with keyboard or pointer input. Selected pages sit in a separate content card; switches, sliders, spacing and color swatches share the shell theme. Changes to LunaDah preferences are saved automatically; destructive system operations remain in the external tools that perform them.

## Coverage

| Page | Direct LunaDah controls | System or host integration / limits |
| --- | --- | --- |
| General | Extensible language drop-down, shell font, 12/24-hour clock, first-run guide, confirmed preference reset | Font choice affects the shell; external application themes remain independent |
| Appearance | Wallpaper image/palettes, LunaDah PNG/JPEG/WebP picker, accent, gaps, panel height, dashboard visibility, smooth blur, opacity and animation duration | Picker uses a bounded QWidget preview and no `QFileDialog`; blur applies to application frames, with no KDE blur protocol |
| Windows and workspaces | 1–9 workspaces, grouped columns, per-column widths, window gaps, default floating mode | Reducing the count moves windows to a remaining workspace |
| Keyboard shortcuts | Click a binding and press the desired Meta-key combination for launch, focus, grouping, resizing, window actions, and all nine workspace switch/move actions | Invalid and duplicate combinations are rejected; press Backspace while recording to disable an action |
| Shell modules | JSON styles, templates, code trust and built-in recovery | [Module schema and contract](MODULES.md); custom QML is not sandboxed |
| Display | Current output information and three nested window sizes | Physical modes, scale, rotation and refresh are managed by the host; host monitor settings can be opened when available. Standalone multi-monitor, HDR and night light remain unavailable |
| Keyboard and pointer | Seven keyboard layouts, repeat rate/delay, cursor size for the next session, input test field | Input-method editor and host mouse/touchpad settings; standalone libinput device configuration remains unavailable |
| Sound | Default output and microphone volume/mute through WirePlumber | Pavucontrol/pwvucontrol handles routing and devices; missing services disable direct controls |
| Network | Current connection state | NetworkManager editor handles Wi-Fi, Ethernet, VPN and saved profiles; LunaDah does not store network passwords |
| Bluetooth | Tool availability and package guidance | Blueman handles pairing and adapters |
| Power and battery | Reported battery charge and supported power profiles | Host power tool handles lid/idle policy. No standalone suspend policy or backlight controls yet |
| Applications and startup | Built-in startup selection, package/plugin/X11 launchers | Default-application editor when installed; arbitrary desktop-entry autostart/session restore is not implemented |
| Privacy and accessibility | Host-identity visibility, reduced motion, native-plugin access | Optional host accessibility/locking settings. LunaDah has no secure lock screen, notification service, screen reader integration or portal permission UI yet |
| Users, date and time | Settings-tool discovery | Installed account/time editors handle authorization; LunaDah does not create users or retain passwords |
| Printers and storage | Settings-tool discovery | system-config-printer and GNOME Disks; their confirmation flows govern destructive actions |
| About | Animated moon artwork, version, development status, OS, kernel, architecture, actual graphics API, GitHub link, and manual update check | Update checks contact the fixed official GitHub Releases endpoint and never install packages; the Discord icon remains disabled until an official invite is published |

An installed executable means the editor can be launched, not that every system service or authorization agent is present. KDE module providers are offered only when their plugin file exists. Host-only tools use the original host environment and are not advertised as native LunaDah hardware controls. They are unavailable in standalone EGLFS sessions.

The internal audio limit is 100%. LunaDah reads service state periodically rather than pretending a requested change succeeded. Helper processes have a 2.5-second timeout and a 64 KiB output cap. Audio requests accept only the default input/output selector and bounded volume or boolean mute; power profiles must appear in the service's supported list. System tool commands come from a fixed allowlist and are never evaluated by a shell. Session actions are separate: logout ends LunaDah, while suspend, reboot and poweroff use logind D-Bus methods only. The menu checks action availability and asks for confirmation before logout, reboot or poweroff; it does not execute shell commands.

## Saved preferences

Additional keys in the existing `[desktop]` group:

| Key | Default | Accepted values |
| --- | --- | --- |
| `workspaceCount` | 4 | Integer 1–9 |
| `masterRatio` | 56 | Legacy saved default width percentage used when creating columns |
| `defaultFloating` | false | Boolean |
| `keyboardLayout` | us | us, gb, de, fr, es, jp, tw |
| `keyRepeatRate` | 25 | Integer 0–60; zero disables repeat |
| `keyRepeatDelay` | 600 | Integer 200–1500 ms |
| `cursorSize` | 24 | Integer 16–64 px; next session |
| `fontFamily` | sans-serif | sans-serif, serif, monospace |
| `clock24Hour` | true | Boolean |
| `startupApps` | empty | Unique built-in IDs: files, console, monitor, welcome |

Startup selection runs after the shell starts in sessions whose first-run guide has been completed. Network, audio, power and account settings belong to their system services and are not copied into LunaDah's INI file. Resetting desktop preferences does not delete documents, reset language/wallpaper selections or modify system-service configuration. Nested window size is session-local.

## IPC and tests

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadahctl open-settings appearance
./build/lunadahctl appearance '{"workspaceCount":6,"masterRatio":60}'
./build/lunadahctl appearance '{"keyboardLayout":"us","keyRepeatRate":25,"keyRepeatDelay":600}'
./build/lunadahctl appearance '{"startupApps":["files"]}'
./build/lunadahctl shortcuts '{"focusLeft":"Meta+U"}'
./build/lunadahctl reset-shortcuts
./build/lunadahctl check-update
./build/lunadahctl desktop-size 1280x720
```

`audio`, `power-profile` and `system-tool` are explicit user actions. Integration tests load every page and reject invalid service commands; they do not alter the host volume, microphone, network, power profile, users or storage. Unit tests cover command validation and helper limits. The old Notes source, entry points and tests are removed; existing user text files are left untouched. IME testing can use the settings input field or Console.

References: [WirePlumber wpctl](https://pipewire.pages.freedesktop.org/wireplumber/man/wpctl.html) and the installed system tools' own help/documentation.

Default terminal and file-manager argument arrays are edited under Applications and startup. Empty arrays select Kitty/Fish and LunaDah Files. See [Default apps and Files](DEFAULT_APPS_AND_FILES.md).

## Window and column controls

The compact top `TopPanel` embeds theme-accented grouped-application cells inline after the workspace/session controls; the former separate `ColumnStrip` is not instantiated as a second layer. The inline area has one cell per column and app icons for all members, including minimized ones. Click an exact icon to restore, focus and reveal that member; drag it onto a member in another column to group it; right-click it or click its minus badge to expel it. A column accepts at most four total windows, including minimized members, and visible members receive equal vertical space.

Defaults use `Super+H/L` between columns, `Super+J/K` within a grouped column, `Super+Shift+H/L` to merge the focused window into the adjacent column, `Super+Shift+E` to expel, `Super+Ctrl+H/L` to reorder columns, Super plus `+`/`-` to resize, and `Super+Shift+C` to center. `Super+C` closes the focused window and the remaining members immediately re-apply the 1/2/3/4 split. `Super+F` maximizes or restores the window under the pointer across the compositor `workArea`, whose top edge is below the top panel's `panelExtent`/exclusive area. Every new window otherwise opens as its own full-width column and the strip slides horizontally to reveal it. The frame's top-right minus and × controls provide quick minimize and close actions.
