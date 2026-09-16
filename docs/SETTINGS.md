# Settings center

Right-click the wallpaper, use the settings side of the panel's centered three-part selector, choose Settings in the launcher, or run `lunadash-desktop --app settings` inside LunaDash. All entries lead to the same Quickshell/QML settings surface. It follows the shell-module JSON dimensions, margins, radius and colors, takes exclusive keyboard focus, and begins below `Theme.barHeight` when using its default position. It closes with Escape from search and provides a top-right quick-hide × control. The sidebar uses consistent outline icons. Search tokenizes the query and indexes translated category names, individual setting names, descriptions and keywords. Ranked setting-level results open the matching page directly with keyboard or pointer input. Selected pages sit in a separate content card; switches, sliders, fields, spacing and color swatches share the shell theme. Text inputs use the shared `SoftField` component and slider rows use the shared `SettingsSlider` component, so a control keeps the same shape on every page instead of each page restyling its own background. Changes to LunaDash preferences are saved automatically. Features that still require a host or external settings editor are intentionally omitted from the visible settings catalog while their JSON/state interfaces remain available for future implementation.

## Coverage

| Page | Direct LunaDash controls | System or host integration / limits |
| --- | --- | --- |
| General | Extensible language drop-down, shell font, 12/24-hour clock, first-run guide, confirmed preference reset | Font choice affects the shell; external application themes remain independent |
| Appearance | Wallpaper image/palettes, in-shell PNG/JPEG/WebP picker, custom `#RRGGBB` accent, gaps, panel height, dashboard visibility, smooth blur, opacity and animation duration | Accent colors can be entered directly; the visible swatches are only quick presets. The picker is drawn inside the settings surface with a bounded preview; blur applies to application frames, with no KDE blur protocol |
| Windows and workspaces | 1–9 workspaces, grouped columns, per-column widths, window gaps, default floating mode | Reducing the count moves windows to a remaining workspace |
| Keyboard shortcuts | Click a binding and press the desired Meta or Alt key combination for launch, focus, grouping, resizing, window actions, all nine workspace switch/move actions, and screen capture | Invalid and duplicate combinations are rejected; press Backspace while recording to disable an action |
| Shell modules | JSON layout, dimensions, positions, colors and built-in recovery | [Module schema and contract](MODULES.md); custom QML modules are not loaded |
| Display | Current output information and three nested window sizes | Physical modes, scale, rotation and refresh are managed by the host; host monitor settings can be opened when available. Standalone multi-monitor, HDR and night light remain unavailable |
| Keyboard and pointer | Seven keyboard layouts, repeat rate/delay, cursor size for the next session, input test field | Input-method editor and host mouse/touchpad settings; standalone libinput device configuration remains unavailable |
| Sound | Default output and microphone volume/mute through WirePlumber | Pavucontrol/pwvucontrol handles routing and devices; missing services disable direct controls |
| Network | Current connection state | NetworkManager editor handles Wi-Fi, Ethernet, VPN and saved profiles; LunaDash does not store network passwords |
| Bluetooth | Tool availability and package guidance | Blueman handles pairing and adapters |
| Power and battery | Reported battery charge and supported power profiles | Host power tool handles lid/idle policy. No standalone suspend policy or backlight controls yet |
| Applications and startup | Built-in startup selection, package/plugin/X11 launchers | Default terminal and file manager chosen from installed applications or a custom argument array; arbitrary desktop-entry autostart/session restore is not implemented |
| Privacy and accessibility | Host-identity visibility, reduced motion, native-plugin access | Optional host accessibility/locking settings. LunaDash has no secure lock screen, notification service, screen reader integration or portal permission UI yet |
| Users, date and time | Settings-tool discovery | Installed account/time editors handle authorization; LunaDash does not create users or retain passwords |
| Printers and storage | Settings-tool discovery | system-config-printer and GNOME Disks; their confirmation flows govern destructive actions |
| About | Animated moon artwork, version, development status, OS, kernel, architecture, actual graphics API, GitHub link, and manual update check | Update checks contact the fixed official GitHub Releases endpoint and never install packages |

An installed executable means the editor can be launched, not that every system service or authorization agent is present. KDE module providers are offered only when their plugin file exists. Host-only tools use the original host environment and are not advertised as native LunaDash hardware controls. They are unavailable in standalone EGLFS sessions.

The internal audio limit is 100%. LunaDash reads service state periodically rather than pretending a requested change succeeded. Helper processes have a 2.5-second timeout and a 64 KiB output cap. Audio requests accept only the default input/output selector and bounded volume or boolean mute; power profiles must appear in the service's supported list. System tool commands come from a fixed allowlist and are never evaluated by a shell. Session actions are separate: logout ends LunaDash, while suspend, reboot and poweroff use logind D-Bus methods only. The menu checks action availability and asks for confirmation before logout, reboot or poweroff; it does not execute shell commands.

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

Startup selection runs after the shell starts in sessions whose first-run guide has been completed. Network, audio, power and account settings belong to their system services and are not copied into LunaDash's INI file. Resetting desktop preferences does not delete documents, reset language/wallpaper selections or modify system-service configuration. Nested window size is session-local.

## IPC and tests

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl open-settings appearance
./build/lunadashctl appearance '{"workspaceCount":6,"masterRatio":60}'
./build/lunadashctl appearance '{"keyboardLayout":"us","keyRepeatRate":25,"keyRepeatDelay":600}'
./build/lunadashctl appearance '{"startupApps":["files"]}'
./build/lunadashctl shortcuts '{"focusLeft":"Meta+U"}'
./build/lunadashctl reset-shortcuts
./build/lunadashctl check-update
./build/lunadashctl send-key copy
./build/lunadashctl desktop-size 1280x720
./build/lunadashctl screenshot
./build/lunadashctl capture /tmp/lunadash.png
```

`send-key` accepts `copy`, `paste`, `cut` or `selectAll` and forwards the shortcut with Control to the focused client. It requires a connected client with keyboard focus and is used by the desktop context menu; it never synthesizes keys without a focused application.

`screenshot` performs the same capture as the `Alt+Shift+F5` shortcut and returns the written path; `capture` writes to an absolute path instead and refuses to overwrite an existing file. Both write one PNG of the current desktop and record it in the `screenCapture.lastCapture` status field. See [Screen capture](SCREEN_CAPTURE.md) for the protocol coverage and its current limits.

`audio`, `power-profile` and `system-tool` are explicit user actions. Integration tests load every page, exercise every option each page can change and reject invalid service commands; they do not alter the host volume, microphone, network, power profile, users or storage. The settings test applies an accepted value and several rejected values to each of the twenty desktop preferences and each of the thirty-nine shortcuts, and requires a rejected value to leave the stored value unchanged. Session actions are only tested with invalid values, so a test run can never suspend or restart the machine. Unit tests cover command validation and helper limits. The old Notes source, entry points and tests are removed; existing user text files are left untouched. IME testing can use the settings input field or Console.

References: [WirePlumber wpctl](https://pipewire.pages.freedesktop.org/wireplumber/man/wpctl.html) and the installed system tools' own help/documentation.

Default terminal and file-manager argument arrays are edited under Applications and startup. Empty arrays select Konsole and LunaDash Files. See [Default apps and Files](DEFAULT_APPS_AND_FILES.md).

## Window and column controls

The compact top `TopPanel` embeds theme-accented grouped-application cells inline after the workspace/session controls; the former separate `ColumnStrip` is not instantiated as a second layer. The inline area has one cell per column and app icons for all members, including minimized ones. Click an exact icon to restore, focus and reveal that member; drag it onto a member in another column to group it; right-click it or click its minus badge to expel it. A column accepts at most four total windows, including minimized members, and visible members receive equal vertical space.

Defaults use `Super+H/L` between columns, `Super+J/K` within a grouped column, `Super+Shift+H/L` to merge the focused window into the adjacent column, `Super+Shift+E` to expel, `Super+Ctrl+H/L` to reorder columns, Super plus `+`/`-` to resize, and `Super+Shift+C` to center. `Super+C` closes the focused window and the remaining members immediately re-apply the 1/2/3/4 split. `Super+F` maximizes or restores the window under the pointer across the compositor `workArea`, whose top edge is below the top panel's `panelExtent`/exclusive area. Every new window otherwise opens as its own full-width column and the strip slides horizontally to reveal it. The frame's top-right minus and × controls provide quick minimize and close actions.
