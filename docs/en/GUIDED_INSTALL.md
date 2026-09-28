# Guided installation and desktop controls

## Install with curl

Use an installed **Arch Linux x86_64** system with Internet access, Bash, curl,
and a normal login account that can use sudo. This is a desktop installer, not a
disk partitioner or an installer for an empty disk. Review downloaded code before
executing it. LunaDash 1.0.1a is still a development desktop.

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh | bash
```

No initial Git checkout is required. The script opens `/dev/tty` for every
question and for interactive child commands; it never reads answers from the
pipe containing its own source. It refuses a real install as root and will not
build AUR recipes with root privileges. If sudo is absent, it can install sudo
using `su`; it does not grant sudo privileges or edit sudoers. A user without
permission must obtain it from their system administrator first.

For review and a no-write preview:

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh -o install.sh
less install.sh
bash install.sh --dry-run
```

The first choice selects **English or Traditional Chinese for this installer**.
It does not change system language, keyboard layout or timezone. Package-manager
output retains the system's own locale. A later, separate step selects a system
UTF-8 locale or keeps the existing one. Chinese, Japanese and Korean selections
include CJK fonts and the appropriate Fcitx add-on; other scripts receive extra
Noto coverage. Linux virtual consoles still need a terminal capable of displaying
the requested characters.

### Five steps

1. Review and install the base package set: more than 20 tools, including glibc,
   Git, curl, certificates, archives, editors, networking, NetworkManager/nmcli,
   firmware, development tools, fonts and D-Bus. Arch receives a full `pacman -Syu`
   rather than an unsupported partial upgrade. If yay is missing, its official
   AUR source is fetched, the recipe is shown, and a separate confirmation is
   required before building it as the login user.
2. Optionally select Chrome, Discord, Zed, Microsoft's Visual Studio Code, OBS,
   Spotify, ProtonPlus, Kitty, Fish, Dolphin, FileZilla, htop, Nmap and Tor. The
   menu supports `all`, `none`, numeric toggles and exclusions such as
   `all -3 -5`, followed by `done`. Installed native/known Flatpak applications
   are marked and not duplicated. Unselected applications are never removed.
   Repository packages use pacman; AUR applications use yay with its normal
   review/confirmation flow. Tor is not automatically enabled as a service.
3. Resolve the selected LunaDash branch to an exact commit, download an HTTPS
   archive for that SHA, validate its paths/size and build a native package as
   the user. Only package installation is privileged. The source archive works
   without `.git`; `.lunadash-revision` records the fetched commit. `--ref SHA`
   can pin the desktop source. This is provenance, not an independent signature
   verification of the initially downloaded installer.
4. Choose a system locale independently, add its fonts/input method support,
   and back up `locale.gen` before changing it. Keyboard layout and timezone are
   not guessed from country. A missing LunaDash UI translation falls back to
   English without changing the chosen system locale.
5. Create the standard Pictures/Wallpapers folder if missing. Keep an existing
   display manager; optionally enable SDDM for the next boot without stopping
   the current session. NetworkManager is enabled only after confirmation and
   only if no conflicting network manager is configured. Finally ask whether to
   reboot. The default is **no**.

Failures stop the sequence and do not reach the reboot question. Private logs
are under `$XDG_STATE_HOME/lunadash` or `~/.local/state/lunadash`, mode 0600.
Temporary source/build files are removed. Installed system packages are not
silently rolled back. Keep the log when reporting an installation failure.

`install.sh` is the new guided entry point. **`scripts/install-session.sh` is
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
