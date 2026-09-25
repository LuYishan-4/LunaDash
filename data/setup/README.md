# First-install templates

The existing `scripts/install-session.sh` opens a terminal guide on a new interactive installation. `--guided` opens it again; `--skip-guide` keeps the direct core installer. Updates and ordinary dry runs never open the guide. The guide installs LunaDash through the existing Arch package or CMake path, then seeds only the selected missing settings.

The optional Arch package groups are defined in `scripts/setup-guide.sh`:

| Group | Purpose |
| --- | --- |
| `basics` | XDG tools, archives, rsync, jq, media control and keyring |
| `desktop` | Chromium, Thunar, archive manager, removable/network file support |
| `media` | MPV, image viewer and video thumbnails |
| `office` | LibreOffice and its Traditional Chinese language pack |
| `development` | Fish, Starship, Fastfetch, eza, fzf, zoxide, tmux and btop |
| `input` | Fcitx5 GTK and Chinese engines, CJK and JetBrains Mono fonts |

Core build/session dependencies remain in `scripts/install-dependencies.sh`. The extra bundles are opt-in and use packages from the configured pacman repositories. Repository availability varies; a package error stops setup with the normal package-manager error. There is no AUR helper bootstrap, repository replacement, default-shell change or automatic service enablement. Other distributions retain the existing core installation path and may use the profiles; install equivalent optional apps with their package manager.

The source reference is the author's `linux-quick-setup-20260924.sh` configuration snapshot (2026-09-24), supplied for this project, plus the [NyxNiri configuration organization](https://github.com/ech678/NyxNiri). The snapshot was read as archive data. Its restore script is never invoked, distributed or fetched by the installer. These small templates re-create the portable Kitty typography/padding/opacity/shortcuts, blue prompt colors and system-summary layout. They carry no account name, hostname, credentials, custom repository, hardware-specific display rule, browser state, wallpaper media or personal project path. LunaDash's existing C++ compositor and Quickshell shell remain authoritative; no Niri, Noctalia, NyxNiri session or X11 window manager is added.

`--desktop-profile` seeds `$XDG_CONFIG_HOME/LuDash/shell-modules.json`; absent `XDG_CONFIG_HOME` defaults to `~/.config`. It uses LunaDash's existing schema and live settings loader. The default dock is disabled by LunaDash preferences; the profile does not disable the dock module, so users can still enable the dock in Settings. Adjust panel fields in Settings or edit this JSON. All unspecified fields inherit the built-in template.

`--author-config` seeds the listed app settings only when the whole starter group is absent. Existing files, directories and symbolic links are preserved. Copies are placed in `$XDG_CONFIG_HOME/LuDash/setup-examples/` for manual comparison; existing examples are preserved too. Make a personal backup before merging examples. The helper rejects symbolic-link parent directories before creating new files. It honors absolute `XDG_CONFIG_HOME` and `XDG_STATE_HOME` paths, including paths containing spaces. Kitty loads `__custom__.conf` last and Fish loads `__custom__.fish` last; those files remain user-owned. Starship and Fastfetch use their regular editable configuration files. The Fish template activates optional tools only when installed and does not select Fish as Kitty's shell.

After successful setup, `$XDG_STATE_HOME/lunadash/setup/complete.json` records the first setup choices and SHA-256 values of created files. It is a record, not a restore program; removal or manual recovery must preserve any subsequent personal edits. First-login Welcome remains available for language and desktop settings. The guide does not bypass system package authorization.
