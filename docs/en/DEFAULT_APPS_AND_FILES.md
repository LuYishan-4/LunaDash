# Default applications, Fish, Files, and image selection

Settings > Applications and startup lets users select a default terminal and file manager. Each action offers a selector of every installed desktop application that the launcher can also find, with a role default pinned at the top (`Kitty (default)` for the terminal, `Dolphin (default)` for files). `[]` selects the role default. Examples are `["kitty"]` and `["dolphin"]`. Save validates the executable and arguments before replacing preferences. These trusted commands run as the current user; argument boundaries are preserved and shell operators are not evaluated.

The native application-role service stores new values as JSON in `${XDG_CONFIG_HOME:-$HOME/.config}/LunaDash/default-applications.json`. Until a role is saved there, it reads existing `defaultApps` entries from the legacy `LunaDash.conf` or `LunaDash.ini` settings file. An explicit empty array selects the LunaDash default for that role.

Super+T and the shell's Terminal buttons use the selected terminal. Super+E and Files buttons use the selected file manager. `lunadash-desktop --app files` also respects the preference and forwards to Dolphin by default. `--path /absolute/folder` appends the folder as one argument. The former built-in manager and its `--builtin` option have been removed. These choices apply to LunaDash launchers, not system-wide MIME associations or every third-party application's embedded terminal.

`lunadash-desktop --app packages` opens the standalone Slint Package Manager. Installed-package and search queries run as the current user. Install, remove and full-upgrade actions show the exact command for review, then open it in the configured terminal only after confirmation. Terminal commands are passed as an argument array without a shell; the execute-option forms for Kitty, Konsole, foot, Alacritty, xterm, GNOME Terminal, GNOME Console and Xfce Terminal are recognized.

## Terminal and Fish

The default terminal is **Kitty**, opened with `Meta+T`. LunaDash leaves Kitty configuration and the login shell unchanged. Non-empty user commands remain supported; explicitly selecting Konsole continues to use its configured profile.

The retired Command Console is no longer a built-in application. Interactive commands use the configured terminal role.

```sh
lunadashctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'  # explicit command example
lunadashctl launch-default terminal
lunadashctl default-apps '{"terminal":[],"files":[]}'                            # Kitty and Dolphin
```

## Portal and wallpaper pickers

The xdg-desktop-portal FileChooser opens the independent `lunadash-filepicker` Slint/Winit Wayland client. It has a bounded frameless surface, navigation, places, list/grid views, hidden-file toggle, search, image preview, manual local-path field, filter and application-choice controls. The helper returns local paths and selected options over a private stdin/stdout exchange; the portal backend constructs the D-Bus response and never evaluates a selected path through a shell.

The browser provides common home folders and media mount shortcuts, back/forward/up navigation, folder search, hidden files, name-sorted list and grid views, and an image preview (64 MiB / 32 megapixel limits). The location field accepts absolute paths, relative paths, `~/` and local `file://` URLs. Enter there navigates or selects a candidate without confirming the request. Ctrl+L focuses Location; Ctrl+F focuses search. The helper validates local paths with `std::filesystem` and never evaluates them through a shell.

Open supports single/multiple files and folders; Save validates the destination and asks before replacing an existing file. The file-type selector honors portal glob/MIME filters, and application-provided choices are displayed and returned. SaveFiles restricts names to basenames and generates unique names for collisions. Invalid paths leave the picker open with an error. Previews are optional: unsupported or oversized images remain selectable as regular files.

The portal D-Bus service exports FileChooser v4 and Settings v2 through GLib GDBus; the file picker itself and the separate ScreenCast selector are standalone Slint/Winit Wayland clients. ScreenCast keeps xdpw's line-based selection contract, displays local screen/window previews where capture is available, and returns the original source label unchanged. Headless Wayland integration exercises the FileChooser UI through OpenFile, SaveFile, SaveFiles and the public portal frontend, and captures its rendered surface. Physical-session visual acceptance remains a separate check.

The wallpaper/calendar picker remains a separate in-shell QML picker.

## Wallpaper picker

The picker lives in the shell at `LunaDash-Shell/imagepicker/ImagePicker.qml`. It is drawn inside the settings surface, so it always appears above the settings content and shares that layer-shell surface, its keyboard focus and its visual style instead of opening a separate window behind the overlay. It lists directories and PNG/JPEG/WebP files through `Qt.labs.folderlistmodel`, offers Home/Pictures/parent navigation plus list and grid views, and requests a bounded 512 × 512 preview. Files above 64 MiB are not selectable. The compositor still validates every path before applying it: `lunadashctl wallpaper-image` rejects unreadable data and images above 32 megapixels.

## Appearance shell integration

The Appearance page opens the picker through the shell's `pickerOpen` property. `choose-wallpaper` is the equivalent IPC entry: it opens the Appearance page and the picker, so remote callers get the same in-shell UI. Confirming selects an absolute local path and sends it through the colocated `lunadashctl wallpaper-image`, which applies the existing wallpaper validation path. Cancellation leaves the wallpaper unchanged and no command shell evaluates the selected path.

## Dolphin

Dolphin is installed as a core runtime dependency and the default files role runs `dolphin --new-window`. Explicitly configured external file-manager commands remain respected. The built-in file manager, private association editor and file-operation engine have been removed; see [Files](FILES.md). FileChooser, the in-shell image picker and the screen/window sharing chooser remain independent and available.

## Verification

Manual acceptance should confirm the default terminal opens Kitty with the existing user configuration and Files opens Dolphin, the default-application selector lists every installed application and switches between them and the pinned role default, list/grid navigation and bounded previews work in the wallpaper picker, oversized or unsupported images cannot be selected, cancellation preserves the wallpaper, and a valid selected path is applied through `lunadashctl wallpaper-image`.
