# Default applications, Fish and Files

Settings > Applications and startup lets users select a default terminal and file manager using JSON argument arrays. `[]` selects the LuDash profile. Examples are `["kitty", "fish"]`, `["konsole", "-e", "fish"]`, and `["dolphin"]`. Save validates the executable and arguments before replacing preferences. These trusted commands run as the current user; argument boundaries are preserved and shell operators are not evaluated. Configuration is per user, under `defaultApps/terminal` and `defaultApps/files` in the LuDash settings file.

Super+Return and the shell's Terminal buttons use the selected terminal. Super+E and Files buttons use the selected file manager. `ludash-desktop --app files` also respects the preference; `--builtin` explicitly opens LuDash Files for recovery. `--path /absolute/folder` navigates the built-in manager or appends the folder as one argument for a custom manager. These choices apply to LuDash launchers, not system-wide MIME associations or every third-party application's embedded terminal. Use the system default-application editor for those associations.

## Terminal profile

The default is **Konsole with interactive Fish**, requiring both `konsole` and `fish` (Arch package dependencies). Fish is a shell; Konsole supplies the terminal emulator, PTY, resizing, colors, and interactive application support. The profile adds a LuDash prompt, working-directory display, exit status, restrained colors and spacing. It is loaded with Fish's `--init-command` after user configuration. It does not run `chsh`, set universal variables, or overwrite `~/.config/fish/`. Selecting a custom command lets you retain your own prompt and profile entirely.

The generated `~/.local/share/konsole/LuDashGenerated.colorscheme` follows the desktop accent on **new terminal windows**. Existing Konsole windows are not recolored live. The Fish source template is `data/terminal/ludash.fish`; the same file is installed in `/usr/share/ludash/terminal/`. LuDash's old Command Console remains a separate non-interactive diagnostic tool (`--app console`), not the default terminal.

```sh
ludashctl default-apps '{"terminal":["kitty","fish"],"files":["dolphin"]}'
ludashctl launch-default terminal
ludashctl default-apps '{"terminal":[],"files":[]}'
```

The current Qt compositor exposes `wl_seat` v4 and `wl_data_device_manager` v1. Recent Foot requires v5 and v3 respectively, so it is not a supported default in this preview. Konsole uses the tested Qt Wayland client path.

## LuDash Files

The native Qt application has a Windows Explorer-inspired layout: a quiet outline-icon toolbar, common locations and mounted-volume sidebar, back/forward/up navigation, editable address bar, current-folder filter, sortable details and a default spacious icon view, hidden-file toggle, and a command toolbar. Desktop color/font changes update already-open Files windows through the shared native application theme. Third-party applications use their own theme integrations.

Available operations: create folder, rename, copy regular files, move files/folders on a filesystem, and move selections to Trash after confirmation. Existing destinations are never overwritten. Copy/move/trash batches run off the UI thread. Operations stop on the first error and report that earlier items may have completed; there is no rollback. Clipboard selection is local to each Files window. Trash restoration, recursive folder copying, cross-filesystem folder moves, archive management, recursive search, network shares, general mounting, cross-application drag/drop and tabs are not implemented. Large operations can continue after closing their window; wait for completion before ending the session.

Shortcuts: Alt+Left/Right for history, Alt+Up for parent, Ctrl+L for location, F2 for rename. Files open with system MIME handlers; executable files require explicitly running them in a terminal. Mounted-volume entries are a startup snapshot of `/run/media`, `/media`, and `/mnt` volumes, not a mount/unmount service.

## Verification

After completing source changes, build and run `ctest --test-dir build --output-on-failure` under Xvfb as described in the testing guide. `files-and-defaults` verifies overwrite prevention, name validation, argument preservation, and rejected recursive launchers. `desktop-interactions` exercises folder navigation. On a system with Quickshell, Konsole, Fish, Xvfb, xdotool and Pillow:

```sh
xvfb-run -a -s '-screen 0 1440x900x24' python3 tests/wayland/test_customization.py build
```

This uses temporary settings, tests module load failure and recovery, launches the actual default Files window, changes its palette, and types a command into interactive Fish. It does not change host defaults, the login shell, or user files.

Profile interfaces follow the [Fish command documentation](https://fishshell.com/docs/4.1/cmds/fish.html) and [Konsole color-scheme format](https://github.com/KDE/konsole/blob/master/data/color-schemes/Breeze.colorscheme).
