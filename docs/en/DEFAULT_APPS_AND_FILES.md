# Default applications, Fish, Files, and image selection

Settings > Applications and startup lets users select a default terminal and file manager. Each action offers a selector of every installed desktop application that the launcher can also find, with a role default pinned at the top (`Kitty (default)` for the terminal, `LunaDash default` for files). `[]` selects the role default. Examples are `["kitty"]` and `["dolphin"]`. Save validates the executable and arguments before replacing preferences. These trusted commands run as the current user; argument boundaries are preserved and shell operators are not evaluated. Configuration is per user, under `defaultApps/terminal` and `defaultApps/files` in the LunaDash settings file.

Super+Return and the shell's Terminal buttons use the selected terminal. Super+E and Files buttons use the selected file manager. `lunadash-desktop --app files` also respects the preference; `--builtin` explicitly opens LunaDash Files for recovery. `--path /absolute/folder` navigates the built-in manager or appends the folder as one argument for a custom manager. These choices apply to LunaDash launchers, not system-wide MIME associations or every third-party application's embedded terminal.

## Terminal and Fish

The default terminal is **Kitty**, opened with `Meta+T` or `Meta+Return`. LunaDash leaves Kitty configuration and the login shell unchanged. Non-empty user commands remain supported; explicitly selecting Konsole continues to use its configured profile.

The retired Command Console is no longer a built-in application. Interactive commands use the configured terminal role.

```sh
lunadashctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'  # explicit command example
lunadashctl launch-default terminal
lunadashctl default-apps '{"terminal":[],"files":[]}'                            # Kitty and built-in Files
```

## Portal and wallpaper pickers

The xdg-desktop-portal FileChooser backend uses a frameless LunaDash-owned shell around the non-native Qt file view. LunaDash owns the dark title/header surface, location field and outer frame, so host decorations cannot reintroduce a white title bar. The location field accepts local absolute paths and `file://` URLs; values are validated by Qt and never evaluated by a shell.

The wallpaper/calendar picker remains a separate in-shell QML picker.

## Wallpaper picker

The picker lives in the shell at `qml/imagepicker/ImagePicker.qml`. It is drawn inside the settings surface, so it always appears above the settings content and shares that layer-shell surface, its keyboard focus and its visual style instead of opening a separate window behind the overlay. It lists directories and PNG/JPEG/WebP files through `Qt.labs.folderlistmodel`, offers Home/Pictures/parent navigation plus list and grid views, and requests a bounded 512 × 512 preview. Files above 64 MiB are not selectable. The compositor still validates every path before applying it: `lunadashctl wallpaper-image` rejects unreadable data and images above 32 megapixels.

## Appearance shell integration

The Appearance page opens the picker through the shell's `pickerOpen` property. `choose-wallpaper` is the equivalent IPC entry: it opens the Appearance page and the picker, so remote callers get the same in-shell UI. Confirming selects an absolute local path and sends it through the colocated `lunadashctl wallpaper-image`, which applies the existing wallpaper validation path. Cancellation leaves the wallpaper unchanged and no command shell evaluates the selected path.

## LunaDash Files

The native Qt application provides back/forward/up navigation, an editable address bar, filtering, sortable details and icon views, hidden-file control, and file operations. Existing destinations are never overwritten. Copy/move/trash batches run off the UI thread and stop on the first error; there is no rollback. Trash restoration, recursive folder copying, cross-filesystem folder moves, archive management, recursive search, network shares, general mounting, cross-application drag/drop and tabs are not implemented.

Files open with system MIME handlers; executable files require explicitly running them in a terminal. The wallpaper picker is a separate, intentionally image-only UI inside settings and does not replace Files or system MIME selection.

## Verification

Manual acceptance should confirm the built-in Terminal starts interactive Fish with the LunaDash profile, the default-application selector lists every installed application and switches between them and the pinned role default, list/grid navigation and bounded previews work in the wallpaper picker, oversized or unsupported images cannot be selected, cancellation preserves the wallpaper, and a valid selected path is applied through `lunadashctl wallpaper-image`.
