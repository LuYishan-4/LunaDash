# Default applications, Fish, Files, and image selection

Settings > Applications and startup lets users select a default terminal and file manager. Each action offers a selector of installed desktop applications, the default terminal (Konsole), the LunaDash Files default, and a Custom command… option for JSON argument arrays. `[]` selects the role default. Examples are `["konsole"]` and `["dolphin"]`. Save validates the executable and arguments before replacing preferences. These trusted commands run as the current user; argument boundaries are preserved and shell operators are not evaluated. Configuration is per user, under `defaultApps/terminal` and `defaultApps/files` in the LunaDash settings file.

Super+Return and the shell's Terminal buttons use the selected terminal. Super+E and Files buttons use the selected file manager. `lunadash-desktop --app files` also respects the preference; `--builtin` explicitly opens LunaDash Files for recovery. `--path /absolute/folder` navigates the built-in manager or appends the folder as one argument for a custom manager. These choices apply to LunaDash launchers, not system-wide MIME associations or every third-party application's embedded terminal.

## Terminal and Fish

The default terminal is **Konsole**. LunaDash reads Konsole's user `konsolerc` `DefaultProfile`, starts a separate Konsole process with that profile, and does not replace its shell, colours, or other user configuration. A user-configured non-empty command remains supported after the existing executable and recursive-launch validation; a plain `["konsole"]` command is normalized to the same user profile.

LunaDash's Command Console remains a separate non-interactive diagnostic tool (`--app console`), not the default terminal.

```sh
lunadashctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'  # custom command example
lunadashctl launch-default terminal
lunadashctl default-apps '{"terminal":[],"files":[]}'                            # Konsole and built-in Files
```

## Wallpaper picker

The picker lives in the shell at `qml/imagepicker/ImagePicker.qml`. It is drawn inside the settings surface, so it always appears above the settings content and shares that layer-shell surface, its keyboard focus and its visual style instead of opening a separate window behind the overlay. It lists directories and PNG/JPEG/WebP files through `Qt.labs.folderlistmodel`, offers Home/Pictures/parent navigation plus list and grid views, and requests a bounded 512 × 512 preview. Files above 64 MiB are not selectable. The compositor still validates every path before applying it: `lunadashctl wallpaper-image` rejects unreadable data and images above 32 megapixels.

## Appearance shell integration

The Appearance page opens the picker through the shell's `pickerOpen` property. `choose-wallpaper` is the equivalent IPC entry: it opens the Appearance page and the picker, so remote callers get the same in-shell UI. Confirming selects an absolute local path and sends it through the colocated `lunadashctl wallpaper-image`, which applies the existing wallpaper validation path. Cancellation leaves the wallpaper unchanged and no command shell evaluates the selected path.

## LunaDash Files

The native Qt application provides back/forward/up navigation, an editable address bar, filtering, sortable details and icon views, hidden-file control, and file operations. Existing destinations are never overwritten. Copy/move/trash batches run off the UI thread and stop on the first error; there is no rollback. Trash restoration, recursive folder copying, cross-filesystem folder moves, archive management, recursive search, network shares, general mounting, cross-application drag/drop and tabs are not implemented.

Files open with system MIME handlers; executable files require explicitly running them in a terminal. The wallpaper picker is a separate, intentionally image-only UI inside settings and does not replace Files or system MIME selection.

## Verification

Manual acceptance should confirm the built-in Terminal starts interactive Fish with the LunaDash profile, the default-application selector lists installed applications and switches between them and the custom command array, list/grid navigation and bounded previews work in the wallpaper picker, oversized or unsupported images cannot be selected, cancellation preserves the wallpaper, and a valid selected path is applied through `lunadashctl wallpaper-image`.
