# Desktop shell

The panel, dock, launcher, dashboard, notifications, lock screen and shell settings come from
the independent [LunaDash-Shell](https://github.com/LunaDash-DE/LunaDash-Shell) repository. It
is written in Slint and runs on [SlintLu](https://github.com/LunaDash-DE/SlintLu), which turns
Slint files into Wayland layer-shell windows and provides the desktop services (audio, network,
MPRIS, notifications, tray, polkit, PAM, idle, clipboard). The compositor talks to it only
through Wayland and its control socket; nothing in LunaDash links a shell toolkit.

## Revisions and installation

`packaging/shell-revision` pins the shell commit and `packaging/slintlu-revision` pins the
SlintLu commit it was tested with. `scripts/fetch-shell.sh` and `scripts/fetch-slintlu.sh` fetch
exactly those commits.

- CMake installs the shell files from `LUDASH_SHELL_SOURCE_DIR` to
  `share/lunadash/shell`. The former name `LUDASH_QML_SOURCE_DIR` still works.
- With `-DLUDASH_SLINTLU_SOURCE_DIR=<checkout>`, CMake builds SlintLu with Cargo and installs it
  as `libexec/lunadash/slintlu`. The Arch package, the source installer and CI always do this;
  the Nix package builds the same revision and links it there.

## Starting the shell

The compositor looks for the shell next to its executable (`../share/lunadash/shell`), then in
the build's shell checkout and the XDG data directories. When the directory has `shell.slint`
it starts `slintlu --config <directory>`, preferring the installed
`libexec/lunadash/slintlu`, then `slintlu` on `PATH`, `~/.cargo/bin` and `~/.local/bin`.
`lunadash-session` refuses to start a session when no SlintLu is available.

`LUDASH_SHELL_RENDERER` (`auto`, `opengl`, `vulkan`, `software`) becomes `SLINTLU_RENDERER`;
`auto` leaves the choice, and the fallback, to SlintLu. See [shell rendering](SHELL_RENDERING.md).

A legacy checkout with `shell.qml` is still started with Quickshell, unchanged.

## What the shell receives from LunaDash

SlintLu recognises LunaDash through its control socket and polls its status:

- Keyboard shortcuts reach the shell as requests: launcher (also the bare Meta toggle),
  settings, wallpaper picker, control center, clipboard and power menu.
- The wallpaper is the compositor's: the shell shows `wallpaperImage`, and choosing a wallpaper
  in the shell sends `wallpaper-image`, `wallpaper-default` or `wallpaper-random`.
- The language follows LunaDash's desktop language, and the shell's language page changes it
  for the desktop (`language`).
- The launcher reports whether it is open (`launcher-visible`), so Meta toggles it correctly.
- Default applications open through `launch-default` (terminal, files, browser).
- On LunaDash the shell does not show its own first-run card; the Slint setup guide does that.

`scripts/ci/check_shell_actions.py` fails when the shell calls a compositor method that has no
handler. `tests/wayland/test_session_shell.py` starts the compositor with the pinned shell and
SlintLu and checks the round trips above (CI runs both).

## Not yet available in the Slint shell

The former Quickshell shell had features that are not ported yet:

- shell modules (`data/modules`, [modules](MODULES.md)) and Quickshell plugins, including
  desktop widgets ([plugins](PLUGINS.md));
- the settings pages driven by the compositor (displays, shortcuts, input, default
  applications, plugins, updates). The shell's own settings cover appearance, wallpaper, panel
  and dock, notifications, power and lock, language and about;
- the session lock and idle lock need `ext-session-lock-v1` and `ext-idle-notify-v1`, which the
  compositor does not offer yet; SlintLu then disables the lock screen.

Those documents still describe the Quickshell implementation.
