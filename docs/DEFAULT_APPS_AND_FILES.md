# Default applications, Fish, Files, and image selection

Settings > Applications and startup lets users select a default terminal and file manager using JSON argument arrays. `[]` selects the LunaDah default. Examples are `["kitty", "fish"]` and `["dolphin"]`. Save validates the executable and arguments before replacing preferences. These trusted commands run as the current user; argument boundaries are preserved and shell operators are not evaluated. Configuration is per user, under `defaultApps/terminal` and `defaultApps/files` in the LunaDah settings file.

Super+Return and the shell's Terminal buttons use the selected terminal. Super+E and Files buttons use the selected file manager. `lunadah-desktop --app files` also respects the preference; `--builtin` explicitly opens LunaDah Files for recovery. `--path /absolute/folder` navigates the built-in manager or appends the folder as one argument for a custom manager. These choices apply to LunaDah launchers, not system-wide MIME associations or every third-party application's embedded terminal.

## Kitty and Fish

The default is **Kitty with interactive Fish**, and the Arch package requires both `kitty` and `fish`. The launch argument vector is equivalent to:

```text
kitty fish --interactive --init-command <LunaDah profile source>
```

The executable paths are resolved before launch. Because Qt Wayland Compositor 6.11 currently exposes `wl_data_device_manager` version 1 while Kitty requests version 3, the built-in default-terminal path launches Kitty through LunaDah's authenticated XWayland compatibility service. The XWayland container starts maximized to the compositor work area. A user-configured terminal command still uses the normal launch path. The profile source comes from the embedded `data/terminal/ludash.fish` resource and is passed as one argument, so shell operators are not re-parsed by an intermediate shell. Fish loads it with `--init-command` after user configuration. LunaDah does not run `chsh`, set universal variables, overwrite `~/.config/fish/`, generate a Kitty or Konsole color scheme, or modify the user's Kitty configuration. A configured non-empty command remains supported and takes precedence after the existing executable and recursive-launch validation.

LunaDah's Command Console remains a separate non-interactive diagnostic tool (`--app console`), not the default terminal.

```sh
lunadahctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'
lunadahctl launch-default terminal
lunadahctl default-apps '{"terminal":[],"files":[]}'
```

## Image picker API

The C++20 QWidget feature is declared in `include/LuDash/file_picker/FilePicker.h`:

```cpp
QString LuDash::selectImageFile(QWidget* parent, const QString& initialPath = {});
bool LuDash::isEligibleImageFile(const QString& path);
QSize LuDash::boundedPreviewSize(const QSize& sourceSize, const QSize& bounds);
```

`selectImageFile` opens a dedicated modal image-selection dialog and returns an absolute local path, or an empty string after cancellation. It does not use `QFileDialog`. The dialog supports typed and parent-directory navigation, PNG/JPEG/WebP-only filtering, list and grid views, filename/path/dimension details, and a preview. Selection rejects missing files, directories, symlinks, unsupported extensions, unreadable image data, files larger than 64 MiB, and images larger than 32 megapixels. Preview decoding requests a maximum 512 × 512 scaled image from `QImageReader`, preventing an unbounded full-resolution preview allocation. `isEligibleImageFile` and `boundedPreviewSize` are public, unit-testable policy helpers.

CMake builds `src/file_picker/FilePicker.cpp` as the Qt Widgets-linked `ludash-file-picker` library, links it into `lunadah-desktop`, and registers `tests/file_picker/FilePickerTests.cpp` as the `file-picker` CTest.

## Appearance shell integration

Quickshell QML cannot call the QWidget API directly. The Appearance page therefore requests:

```qml
shell.command("choose-wallpaper", "")
```

The compositor handles `choose-wallpaper` by launching `lunadah-desktop --app image-picker`. That QWidget-capable process calls `LuDash::selectImageFile(nullptr)`. Cancellation exits without changing the wallpaper; after confirmation, it URL-encodes the local path and executes the colocated `lunadahctl wallpaper-image`, which applies the existing wallpaper validation path. No command shell evaluates the selected path.

## LunaDah Files

The native Qt application provides back/forward/up navigation, an editable address bar, filtering, sortable details and icon views, hidden-file control, and file operations. Existing destinations are never overwritten. Copy/move/trash batches run off the UI thread and stop on the first error; there is no rollback. Trash restoration, recursive folder copying, cross-filesystem folder moves, archive management, recursive search, network shares, general mounting, cross-application drag/drop and tabs are not implemented.

Files open with system MIME handlers; executable files require explicitly running them in a terminal. The image picker is a separate, intentionally image-only API and does not replace Files or system MIME selection.

## Verification

The focused `default-applications` and `file-picker` CTests build from `tests/default_applications/DefaultApplicationsTests.cpp` and `tests/file_picker/FilePickerTests.cpp`. The picker helper tests cover aspect-ratio-bounded sizing and rejection of directories, missing paths, and supported image data with a disguised unsupported extension. Existing `files-and-defaults` coverage continues to verify argument preservation and rejected recursive launchers.

Manual acceptance should confirm Kitty starts interactive Fish with the LunaDah profile, no `LuDashGenerated.colorscheme` is created, list/grid navigation works, oversized or unsupported images cannot be selected, previews remain bounded, cancellation preserves the wallpaper, and a valid selected path is applied through `lunadahctl wallpaper-image`.
