# Guided installation and desktop controls

## Install with curl

Use an installed **Arch Linux x86_64** system with Internet access, Bash, curl,
and a normal login account that can use sudo. This is a desktop installer, not a
disk partitioner or an installer for an empty disk. Review downloaded code before
executing it. LunaDash 1.0.2a is still a development desktop.

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh | bash
```

No Git checkout is required. The piped script is a bootstrap: it installs Git if
it is missing, fetches the LunaDash source into `~/.cache/lunadash/source`
(`$XDG_CACHE_HOME` when set), links the `lunadash` command into `~/.local/bin`
and continues with the `install.sh` of that checkout. The branch (default `dev`,
or `--ref`) is resolved to an exact commit on GitHub, and the fetched tree must
match that commit.

If GitHub cannot be reached, the source is fetched through the
`https://gh-proxy.org/` mirror. LunaDash-Shell and SlintLu are always fetched by
the commit pinned in `packaging/*-revision`, so a mirror cannot change them. For
the LunaDash branch itself, a mirror is only trusted for the commit id when
GitHub does not answer at all; the installer then says so.
`LUNADASH_REPO=URL` uses only that repository (a fork or local mirror);
`LUNADASH_GIT_MIRRORS` replaces the mirror list for the pinned components
(space-separated URL prefixes, empty for GitHub only).

The script opens `/dev/tty` for every question and for interactive child
commands; it never reads answers from the pipe containing its own source. It
refuses to run as root and will not build AUR recipes with root privileges. If
sudo is absent, it can install sudo using `su`; it does not grant sudo privileges
or edit sudoers.

For review and a no-write preview:

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh -o install.sh
less install.sh
bash install.sh --dry-run
```

The installer language follows `LANG`: any Chinese locale shows Traditional
Chinese, everything else English; `--language en|zh_TW` overrides it. It does not
change system language, keyboard layout or timezone. A later, separate step
selects a system UTF-8 locale or keeps the existing one. Chinese, Japanese and
Korean selections include CJK fonts and the appropriate Fcitx add-on; other
scripts receive extra Noto coverage.

### After the installation

```bash
lunadash update     # newest source of the installed branch, system upgrade, rebuild
lunadash install    # run the guided installation again
lunadash status     # cached source, branch, commit and installed package
```

`lunadash update` asks no installer questions: it fetches the branch recorded at
installation (or `--ref`), runs `pacman -Syu` (which still asks for confirmation)
and rebuilds and reinstalls the package. An existing `~/.local/bin/lunadash` that
is not this link is kept. `install.sh` run from any other LunaDash checkout
installs that checkout as it is, without fetching or linking the command.

### Five steps

The installer only asks where a choice is needed; pacman and yay still list
packages and confirm before they change the system.

1. Install the base package set together with a full `pacman -Syu` (no
   unsupported partial upgrade): more than 20 tools, including glibc, Git, curl,
   certificates, archives, editors, NetworkManager/nmcli, firmware, development
   tools, fonts and D-Bus.
2. The application menu: Chrome, Discord, Zed, Microsoft's Visual Studio Code,
   OBS, Spotify, ProtonPlus, Kitty, Fish, Dolphin, FileZilla, htop, Nmap and Tor.
   Nothing is selected at first; numbers toggle, `all`, `none` and exclusions
   such as `all -3 -5` work, and Enter continues. Installed native/known Flatpak
   applications are marked and not duplicated; unselected applications are never
   removed. Repository packages use pacman, AUR applications use yay. Only when an
   AUR application is selected and yay is missing is its official AUR recipe
   fetched and shown, with one confirmation before it is built as the login user.
   Tor is not enabled as a service.
3. Build the native package as the user from the cached source, with the
   LunaDash shell and SlintLu at their pinned revisions. Only package
   installation is privileged. Sessions start the LunaDash shell unless you chose
   otherwise: `--shell custom --shell-command waybar` or `--shell none` writes
   `~/.config/LunaDash/session.json` ([desktop shell](SHELL.md#choosing-a-shell));
   without `--shell` an existing choice there is kept.
4. Choose a system locale (Enter keeps the current one), add its fonts/input
   method support, and back up `locale.gen` before changing it. Keyboard layout
   and timezone are not guessed from country. A missing LunaDash UI translation
   falls back to English without changing the chosen system locale.
5. Create the standard Pictures/Wallpapers folder if missing. Keep an existing
   display manager; when there is none, ask once whether to enable SDDM for the
   next boot without stopping the current session. NetworkManager is enabled for
   the next boot when no other network service (systemd-networkd, ConnMan, iwd,
   dhcpcd) is configured; the live connection is not touched. The installer never
   reboots; it prints whether to log out or reboot.

Failures stop the sequence. Private logs are under `$XDG_STATE_HOME/lunadash` or
`~/.local/state/lunadash`, mode 0600. Temporary build files are removed; the
source cache is kept for `lunadash update`. Installed system packages are not
silently rolled back. Keep the log when reporting an installation failure.

`install.sh` is the guided entry point. **`scripts/install-session.sh` is
unchanged** and remains available for existing developer/test workflows. Other
distributions still use the documented source/dependency workflow; this guided
application list does not claim cross-distribution support.

## Wallpaper gallery

**Super+W** toggles a separate, template-capable thumbnail gallery. It no longer
opens the settings window or an arbitrary-path chooser. The default directory
comes from Pictures/Wallpapers; edit it with the folder button. Dropping images
or videos there makes them available on the next discovery refresh (up to five
seconds); the refresh button forces a rescan. Discovery is bounded to 512 images
plus recent/favorite items, with one level of category directories. Directory
symlinks are not recursively followed.

The popup has search, category/media filters, Library/Bundled/Favorites tabs,
light/dark/auto controls and instant selection. Search receives focus on open;
Enter applies the selected result, arrows navigate, Escape clears search then
closes, and the same shortcut or a click outside dismisses it. Favorites persist
across library changes. Video entries use poster images, never `Image` decoding
of an MP4. Filesystem paths are sent as data, not interpolated shell commands.
The replacement extension target is `wallpaper-gallery` (SDK 2).

## Control Center and Settings

The Control Center's Display, Network, Bluetooth, Power, Appearance and system
entries open adjustable pages **inside the Control Center**. Only its upper-right
settings gear opens the full Settings window. Back/Escape returns to the home
page. Privileged tools on the system page retain their existing authorization
flow rather than introducing password storage in the shell.

Settings opens/closes with the shared animation template; maximize/restore
interpolates bounded work-area geometry. The About page combines version/revision,
hardware counters, update status and source/community links. Unavailable GPU
counters are shown as unavailable, not fabricated as zero activity.

Settings, Control Center and the gallery use the compositor's real lower-scene
backdrop through `WindowGlass`, excluding their own text/content. Transparent
QML surfaces are enabled only while that backdrop is ready. Disabled blur,
renderer failure and eye-care mode use opaque readable fallbacks. Reduced-motion
preferences govern transitions. This does not depend on KWin or KDE blur APIs.

## Validation scope

Tests cover curl-pipe/PTY isolation, language independence, package selections,
archive packaging with and without Git, wallpaper discovery/favorites, bounded
models, glass fallbacks, native backdrop ownership and live Quickshell screenshots.
Headless/CI rendering is not a substitute for testing proprietary GPU drivers,
network changes, a full fresh-machine installation or reboot on physical hardware.
