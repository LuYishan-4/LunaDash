# Default applications, Fish, Files, and image selection

Settings > Applications and startup lets users select a default terminal and file manager. Each action offers a selector of every installed desktop application that the launcher can also find, with a role default pinned at the top (`Kitty (default)` for the terminal, `Dolphin (default)` for files). `[]` selects the role default. Examples are `["kitty"]` and `["dolphin"]`. Save validates the executable and arguments before replacing preferences. These trusted commands run as the current user; argument boundaries are preserved and shell operators are not evaluated. Configuration is per user, under `defaultApps/terminal` and `defaultApps/files` in the LunaDash settings file.

Super+Return and the shell's Terminal buttons use the selected terminal. Super+E and Files buttons use the selected file manager. `lunadash-desktop --app files` also respects the preference and forwards to Dolphin by default. `--path /absolute/folder` appends the folder as one argument. The former built-in manager and its `--builtin` option have been removed. These choices apply to LunaDash launchers, not system-wide MIME associations or every third-party application's embedded terminal.

## Terminal and Fish

The default terminal is **Kitty**, opened with `Meta+T` or `Meta+Return`. LunaDash leaves Kitty configuration and the login shell unchanged. Non-empty user commands remain supported; explicitly selecting Konsole continues to use its configured profile.

The retired Command Console is no longer a built-in application. Interactive commands use the configured terminal role.

```sh
lunadashctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'  # explicit command example
lunadashctl launch-default terminal
lunadashctl default-apps '{"terminal":[],"files":[]}'                            # Kitty and Dolphin
```

## Portal and wallpaper pickers

The xdg-desktop-portal FileChooser backend uses one LunaDash-owned `FilePickerDialog`, with a frameless dark header, resize grip and maximize/restore control. Its file model and selection belong to this dialog; no stack-allocated `QFileDialog` is reparented into a shorter-lived wrapper. Results are copied before the dialog is destroyed, including on repeated requests.

The browser provides standard home folders and mounted-media shortcuts, back/forward/up navigation, folder search, hidden files, sortable details and icon views, and an image preview (64 MiB / 32 megapixel decode limits). The location field accepts absolute paths, relative paths, `~/` and local `file://` URLs. Enter in that field navigates or selects a candidate without confirming the request. Ctrl+L focuses Location; Ctrl+F focuses search. Paths are validated with Qt and never evaluated by a shell.

Open supports single/multiple files and folders; Save validates the destination and asks before replacing an existing file. The file-type selector honors portal glob/MIME filters, and application-provided choices are displayed and returned. SaveFiles restricts names to basenames and generates unique names for collisions. Invalid paths leave the picker open with an error. Previews are optional: unsupported or oversized images remain selectable as regular files.

The Main Qt workflow exercises repeated acceptance, multiple selection, filters, manual paths, saving and invalid paths, and retains a picker screenshot. This is automated coverage; visual acceptance in the actual desktop session remains a separate manual check.

The wallpaper/calendar picker remains a separate in-shell QML picker.

## Wallpaper picker

The picker lives in the shell at `qml/imagepicker/ImagePicker.qml`. It is drawn inside the settings surface, so it always appears above the settings content and shares that layer-shell surface, its keyboard focus and its visual style instead of opening a separate window behind the overlay. It lists directories and PNG/JPEG/WebP files through `Qt.labs.folderlistmodel`, offers Home/Pictures/parent navigation plus list and grid views, and requests a bounded 512 × 512 preview. Files above 64 MiB are not selectable. The compositor still validates every path before applying it: `lunadashctl wallpaper-image` rejects unreadable data and images above 32 megapixels.

## Appearance shell integration

The Appearance page opens the picker through the shell's `pickerOpen` property. `choose-wallpaper` is the equivalent IPC entry: it opens the Appearance page and the picker, so remote callers get the same in-shell UI. Confirming selects an absolute local path and sends it through the colocated `lunadashctl wallpaper-image`, which applies the existing wallpaper validation path. Cancellation leaves the wallpaper unchanged and no command shell evaluates the selected path.

## Dolphin

Dolphin is installed as a core runtime dependency and the default files role runs `dolphin --new-window`. Explicitly configured external file-manager commands remain respected. The built-in file manager, private association editor and file-operation engine have been removed; see [Files](FILES.md). FileChooser, the in-shell image picker and the screen/window sharing chooser remain independent and available.

## Verification

Manual acceptance should confirm the default terminal opens Kitty with the existing user configuration and Files opens Dolphin, the default-application selector lists every installed application and switches between them and the pinned role default, list/grid navigation and bounded previews work in the wallpaper picker, oversized or unsupported images cannot be selected, cancellation preserves the wallpaper, and a valid selected path is applied through `lunadashctl wallpaper-image`.
