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
When session.json selects the LunaDash shell, `lunadash-session` refuses to start a session without SlintLu (`lunadash-compositor --session-shell` prints the selection).

`LUDASH_SHELL_RENDERER` (`auto`, `opengl`, `vulkan`, `software`) becomes `SLINTLU_RENDERER`;
`auto` leaves the choice, and the fallback, to SlintLu. See [shell rendering](SHELL_RENDERING.md).

A legacy checkout with `shell.qml` is still started with Quickshell, unchanged.

## Choosing a shell

Like Hyprland's `exec-once`, a session can run the LunaDash shell, another shell or none.
`~/.config/LunaDash/session.json` decides; without it the first
`$XDG_CONFIG_DIRS/LunaDash/session.json` (normally `/etc/xdg`) applies, and without either the
LunaDash shell starts.

```json
{
  "shell": "custom",
  "shellCommand": ["waybar"],
  "launcherCommand": ["fuzzel"],
  "autostart": [["mako"], ["swaybg", "-i", "/home/me/Pictures/wall.png"]]
}
```

- `shell`: `lunadash` (default), `custom` (runs `shellCommand`) or `none`.
- `launcherCommand`: what Super and the launcher shortcut start when the LunaDash shell is not
  running. Other shell shortcuts (control center, clipboard, power menu) do nothing then.
- `autostart`: up to 32 commands started once with the session, whatever the shell.
- Commands are argument arrays and run without a command shell; use `["sh", "-c", "..."]` for
  pipes or variables.
- The Welcome app and setup guide only open with the LunaDash shell.
- `LUNADASH_SHELL=none` or `LUNADASH_SHELL=lunadash` overrides the file for one session.

A file with an unknown key, a wrong type or a custom shell without `shellCommand` is ignored
as a whole: the session falls back to the LunaDash shell and logs why. `lunadashctl status`
shows the active configuration and any error under `session`.

The installers write this file: `install.sh --shell custom --shell-command waybar`, or
`scripts/install-session.sh --shell none`, and both ask during a guided installation. They
replace only `shell` (and `shellCommand`) and keep your other keys. With `custom` or `none`,
`install-session.sh` builds LunaDash with `-DLUDASH_INSTALL_SHELL=OFF`: no shell files and no
SlintLu are built or installed, and `/etc/xdg/LunaDash/session.json` defaults to `none`. The
Arch package always contains the LunaDash shell.

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
- On LunaDash the shell does not show its own first-run card. Until setup is finished, the
  compositor opens the Welcome app at session start (and again after `lunadashctl setup`);
  its "Start desktop" button or Enter finishes setup.
- Shell surfaces set their pointer through `wp_cursor_shape_v1` and render at fractional scales
  through `wp_fractional_scale_v1`. The compositor shows the default arrow whenever a surface
  without a cursor of its own is under the pointer, also right after a surface such as the
  `slurp` selector closes.

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
