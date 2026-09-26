# Settings center

Right-click the wallpaper, use the panel entry, choose Settings in the launcher, or run `lunadash-desktop --app settings` inside LunaDash. All entries lead to the same Quickshell/QML settings surface. In 1.0.1a search lives in the top header and the surface can switch between its default size and a maximized layout. It follows the shell-module JSON dimensions, margins, radius and colors, takes exclusive keyboard focus, and begins below `Theme.barHeight` when using its default position. It closes with Escape from search and provides a top-right quick-hide × control. The sidebar uses consistent outline icons. Search tokenizes the query and indexes translated category names, individual setting names, descriptions and keywords. Ranked setting-level results open the matching page directly with keyboard or pointer input. Selected pages sit in a separate content card; switches, sliders, fields, spacing and color swatches share the shell theme. Text inputs use the shared `SoftField` component and slider rows use the shared `SettingsSlider` component, so a control keeps the same shape on every page instead of each page restyling its own background. Changes to LunaDash preferences are saved automatically. Features that still require a host or external settings editor are intentionally omitted from the visible settings catalog while their JSON/state interfaces remain available for future implementation.

Page content scrolls vertically with the mouse wheel, touch gestures or the scrollbar, including plugin replacements and expanded JSON editors. Changing pages resets the scroll position. The Plugins sidebar page owns plugin enable/disable, composition mode, categorized options and advanced JSON; launcher/dashboard plugin shortcuts and `lunadash-desktop --app plugins` open that same page.

Press **Ctrl+F** anywhere in Settings to focus the header search and select its current text, including while editing a JSON field. This shortcut is inactive while the image picker is open.

The image picker focuses and selects its path field when opened, and Tab stays within the picker. Closing it returns focus to the header search if Settings remains open.

## Coverage

| Page | Direct LunaDash controls | System or host integration / limits |
| --- | --- | --- |
| General | Extensible language drop-down, shell font, 12/24-hour clock, welcome screen, confirmed preference reset | Font choice affects the shell; external application themes remain independent |
| Appearance | Wallpaper image/palettes, in-shell PNG/JPEG/WebP picker, custom `#RRGGBB` accent, gaps, panel height, dashboard visibility, smooth blur, opacity and animation duration | Accent colors can be entered directly; the visible swatches are only quick presets. The picker is drawn inside the settings surface with a bounded preview; blur applies to application frames, with no KDE blur protocol |
| Windows and workspaces | 1–10 workspaces, bounded split tiles, eight-member vertical groups and window gaps | New windows open tiled and non-maximized; `Meta+F` maximizes one window or restores all workspace tiles |
| Keyboard shortcuts | Click a binding and press the desired Meta or Alt key combination for launch, focus, grouping, resizing, window actions, all ten workspace switch/move actions, and screen capture | Invalid and duplicate combinations are rejected; press Backspace while recording to disable an action |
| Plugins | Native, Quickshell and OpenGL extensions; per-feature built-in settings, plugin options and JSON | Hot reload, disabled native defaults and built-in recovery; [plugin SDK](PLUGINS.md) |
| Shell modules | JSON layout, dimensions, positions, colors and built-in recovery | [Module schema and contract](MODULES.md); custom QML modules are not loaded |
| Display | Backlight and per-monitor DDC/CI brightness, primary-output resolution/refresh rate and 100–300% scale | Mode changes require confirmation within 15 seconds; nested physical modes belong to the host. Multi-monitor arrangement, rotation, HDR and night light remain unavailable |
| Keyboard and pointer | Seven keyboard layouts, repeat rate/delay, cursor size for the next session, input test field | Input-method editor and host mouse/touchpad settings; standalone libinput device configuration remains unavailable |
| Sound | Default output and microphone volume/mute through WirePlumber | Pavucontrol/pwvucontrol handles routing and devices; missing services disable direct controls |
| Network | Current connection state | NetworkManager editor handles Wi-Fi, Ethernet, VPN and saved profiles; LunaDash does not store network passwords. Saved profiles and Wi-Fi names that contain a colon are still listed and matched correctly |
| Bluetooth | Tool availability and package guidance | Blueman handles pairing and adapters |
| Power and battery | Reported battery charge and supported power profiles | Host power tool handles lid/idle policy. No standalone suspend policy or backlight controls yet |
| Applications and startup | Built-in startup selection, package/plugin/X11 launchers | Default terminal and file manager chosen from installed applications or a custom argument array; arbitrary desktop-entry autostart/session restore is not implemented |
| Privacy and accessibility | Host-identity visibility, reduced motion, native-plugin access | Optional host accessibility/locking settings. LunaDash has no secure lock screen, notification service, screen reader integration or portal permission UI yet |
| Users, date and time | Settings-tool discovery | Installed account/time editors handle authorization; LunaDash does not create users or retain passwords |
| Printers and storage | Settings-tool discovery | system-config-printer and GNOME Disks; their confirmation flows govern destructive actions |
| About | Animated moon artwork, version, development status, OS, kernel, architecture, actual graphics API, GitHub link, and manual update check | Checks use the fixed official GitHub release/commit endpoints; installation and rollback require a separate user action |

An installed executable means the editor can be launched, not that every system service or authorization agent is present. KDE module providers are offered only when their plugin file exists. Host-only tools use the original host environment and are not advertised as native LunaDash hardware controls. They are unavailable in standalone DRM/KMS sessions.

The internal audio limit is 100%. LunaDash reads service state periodically rather than pretending a requested change succeeded. Helper processes have a 2.5-second timeout and a 64 KiB output cap. Audio requests accept only the default input/output selector and bounded volume or boolean mute; power profiles must appear in the service's supported list. System tool commands come from a fixed allowlist and are never evaluated by a shell. Session actions are separate: logout ends LunaDash, while suspend, reboot and poweroff use logind D-Bus methods only. The menu checks action availability and asks for confirmation before logout, reboot or poweroff; it does not execute shell commands.

## Saved preferences

Additional keys in the existing `[desktop]` group:

| Key | Default | Accepted values |
| --- | --- | --- |
| `workspaceCount` | 10 | Integer 1–10 |
| `keyboardLayout` | us | us, gb, de, fr, es, jp, tw |
| `keyRepeatRate` | 25 | Integer 0–60; zero disables repeat |
| `keyRepeatDelay` | 600 | Integer 200–1500 ms |
| `cursorSize` | 24 | Integer 16–64 px; next session |
| `fontFamily` | sans-serif | sans-serif, serif, monospace |
| `clock24Hour` | true | Boolean |
| `startupApps` | empty | Unique built-in IDs: files, packages, welcome |

Startup selection runs after the shell starts in sessions whose welcome screen has been dismissed. Network, audio, power and account settings belong to their system services and are not copied into LunaDash's INI file. Resetting desktop preferences does not delete documents, reset language/wallpaper selections or modify system-service configuration. Nested window size is session-local.

## IPC and tests

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl open-settings appearance
./build/lunadashctl appearance '{"workspaceCount":6,"gap":8}'
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

`screenshot` starts the same region selection as `Meta+Shift+S` and returns immediately with a pending state. Drag to select or press Escape to cancel; the saved path appears in `screenCapture.lastCapture`. `capture` retains explicit full-output capture to an absolute path and refuses to overwrite an existing file. See [Screen capture](SCREEN_CAPTURE.md) for the protocol coverage and its current limits.

`audio`, `power-profile` and `system-tool` are explicit user actions. Integration tests load every page, exercise every option each page can change and reject invalid service commands; they do not alter the host volume, microphone, network, power profile, users or storage. The settings test applies an accepted value and several rejected values to each of the twenty desktop preferences and each current configurable shortcut, and requires a rejected value to leave the stored value unchanged. Session actions are only tested with invalid values, so a test run can never suspend or restart the machine. Unit tests cover command validation and helper limits. The old Notes and Command Console sources, entry points and tests are removed; existing user files are left untouched. IME testing uses Settings fields or a configured terminal/application.

References: [WirePlumber wpctl](https://pipewire.pages.freedesktop.org/wireplumber/man/wpctl.html) and the installed system tools' own help/documentation.

Default terminal, file-manager and browser argument arrays are edited under Applications and startup. Empty arrays select Kitty, Dolphin and the detected default browser. Kitty opens with Meta+T by default and keeps its existing configuration. These role settings do not replace file-type associations. See [Default apps and Files](DEFAULT_APPS_AND_FILES.md).

## Window and column controls

The top panel groups mapped application windows into one capsule per workspace, including minimized and temporarily hidden windows. Each app remains independently clickable; capsules are independent of tiling columns. The active workspace uses a brighter accent tint, other capsules use a darker shade, and app icons have no numeric workspace badges. Click empty capsule padding to switch workspace without changing its layout. Clicking a tiled task maximizes it and temporarily hides the other windows in its workspace. Those windows remain available in the taskbar. Clicking the active maximized task again, using the application restore button or pressing `Meta+F` restores the saved tiling layout and sizes. A task selected from another workspace first switches to that workspace with an animation. The separate `ColumnStrip` is not instantiated by default.

Columns tile vertically with at most eight total members, including minimized windows. All tiles stay within one screen. A new window splits the largest tile along its longer edge, keeping a stable arrangement. Ordinary windows cannot float or overlap; transient dialogs remain above their parent. Alt + left-drag swaps slots at a target's center, or inserts beside its top/bottom edge. Slot swapping preserves slot sizes. With one active window, Alt dragging translates it without resizing or leaving the work area. Shift + Alt + left-drag adjusts shared split boundaries and grouped row heights while redistributing remaining height without reordering. `Meta+Shift+H/L` merges into a neighboring column; `Meta+Shift+E` separates a member into its own column.

Alt+Tab previews windows in the current workspace. Super+Tab opens the workspace overview. Tab/Shift+Tab, arrow keys and scrolling move the selection; releasing Alt or Super confirms the matching switcher, and Esc cancels. Selecting an unused workspace expands a smaller configured workspace count as needed. `Meta+0` and `Meta+Shift+0` address workspace 10. The taskbar uses workspace capsules instead of numeric labels and switches there with an animation before focusing and enlarging a selected task.

Thumbnails are rendered to bounded 320×200 buffers before readback, then encoded outside the input thread. They refresh when the overview opens; unavailable content uses an application icon. Theme-colored rounded outlines indicate tiled-window focus. These outlines do not mask client content into rounded corners.

`defaultFloating`, `altMouseResize` and the floating-toggle shortcut are retired. See [window interaction validation](WINDOWS.md) for behavior and validation limits.

Defaults use `Super+H/L` to focus left/right and `Super+J/K` to focus down/up, `Super+Shift+H/L` to merge the focused window into the adjacent column, `Super+Shift+E` to expel, `Super+Ctrl+H/L` to exchange neighboring groups, Super plus `+`/`-` to resize, and `Super+Shift+C` to center a single window. `Super+C` closes the focused window and the remaining members share the available height. The first window uses its requested size, proportionally fitted to the screen when necessary. Later windows split existing tiles; clients redraw at the requested tile size. The old default column-width setting no longer applies. New windows restore the tiled arrangement instead of opening maximized. `Super+F` maximizes the focused tiled window to the work area and hides its workspace peers; pressing it again restores the saved tiling arrangement without changing saved split ratios or row heights. Dialogs belonging to the maximized window remain usable. Closing or minimizing it also reveals the other tiles. Each Super+Tab workspace cell reflects its tiled or maximized presentation; Alt+Tab instead shows the current workspace's windows. Pointer clicks explicitly synchronize the selected client, keyboard focus and stacking order so typing stays on the surface the user clicked.

## Display and startup controls

See [display controls and startup](DISPLAY_AND_STARTUP.md) for backlight/DDC/CI brightness, primary-output resolution/refresh/scale, automatic rollback and the session loading animation. Translated settings labels wrap within their controls and cards instead of retaining a fixed single-line height.

About displays the fixed tagline `⑨ baka ᗜˬᗜ` in every language. It is a brand string, not a translated description.

### Update status

About separates one current update status from the last installation record. The running version and latest checked version are labeled separately; an unchecked channel displays no invented latest version. Progress appears only while the installer is active. A completed update from this session requests a restart and cannot be installed again for the same target. Old success/failure records remain in history without hiding a new check. Restarting the compositor clears the restart prompt for previous-session installations, including rollbacks. Switching channels discards any in-flight response from the previous channel.

Settings components in directories with a `qmldir` must be declared there, including helper objects used by loaded pages. The QML design-system check validates these declarations. Selecting the currently loaded category keeps its content visible instead of waiting for a new Loader event.

Background updates run the installer in non-interactive update mode: package confirmation is automatic, but administrator authorization is not bypassed. At 82%, About shows **Waiting for authorization** and the settings surface yields its keyboard focus/layer to the system dialog. It advances to 84% only after authorization succeeds and system installation starts. Without a registered desktop polkit agent, the update fails with a diagnostic instead of opening an invisible terminal prompt. Manual `./scripts/install-session.sh` installation retains normal terminal confirmation. `--non-interactive --skip-deps` is available for pre-provisioned systems; it does not configure SDDM or autologin. Without graphical elevation, it requires already-authorized non-interactive sudo/doas.

LunaDash starts a distribution-provided polkit authentication agent in its login session. Graphical updates also start an available agent before building, so existing sessions can update without logging out first. Arch installation includes `polkit-kde-agent`; other distributions must install a KDE, LXQt, or GNOME polkit agent. The daemon/package `polkit` alone does not provide a graphical password dialog. Existing registered agents are not replaced, and the installer stops only its own temporary agent when it exits.

The desktop does not automatically reload its QML files during package replacement. Log out and back in after installing an update (or reboot) to load the complete new shell. Developers can opt into file watching with `LUNADASH_QML_WATCH=1` before starting the shell; starting an About update or rollback disables watching for the rest of that shell process.

## Wallpaper transitions

Changing the selected wallpaper animates the large preview. Applying it uses the same transition on the desktop: the old image remains visible until the new image loads, then a circle grows from the bottom-right corner to cover it. Rapid selections finish at the latest choice, and failed loads keep the previous image. The GPU path uses a masked image; the software-rendering fallback clips the same circle in a canvas. Turning animations off applies a ready image immediately.

See [Media](MEDIA.md) for the player controls and lyrics area, and [Window layout templates](WINDOW_LAYOUT_TEMPLATES.md) for the reserved stacking extension point.


## 1.0.1a visual layout rules

Dashboard and Settings share the same strong/glass surface hierarchy. Dashboard uses a bounded 12-column layout for the hero, quick actions, four telemetry cards, active-window strip and desktop-status card; content may shrink but must not overlap or escape its card. Settings uses a glass header with search, category badge, version, maximize/restore and close controls.

The old standalone “Motion” wording is retired. Appearance exposes **Visual effects**, while **Reduced motion** remains the accessibility preference. Settings no longer exposes the removed Command Console.

## NyxNiri-inspired desktop, Orbit and live wallpapers

See [the desktop integration guide](NYXNIRI_DESKTOP.md) for the new modules, palette and portal services, shortcuts, dependencies, and current verification limits.
