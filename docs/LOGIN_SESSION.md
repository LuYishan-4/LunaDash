# Boot and login session

[Traditional Chinese guide](LOGIN_SESSION.zh-TW.md)

LunaDash is a development preview. The installer registers a real Wayland login entry, but nested testing does not prove that every physical DRM/KMS GPU, input-seat, VT or display-manager combination works. Keep another desktop or TTY available during evaluation.

## Supported installer paths

Run the installer from the checkout as your normal user:

```sh
./scripts/install-session.sh --dry-run
./scripts/install-session.sh
```

The same script supports these package-manager families:

| Distribution family | Build/install path |
| --- | --- |
| Arch Linux and derivatives | `makepkg --syncdeps --force --install`; pacman owns installed files |
| Debian / Ubuntu and derivatives | apt dependencies, Ninja build, `cmake --install /usr` |
| Fedora and derivatives | dnf dependencies, Ninja build, `cmake --install /usr` |
| openSUSE Tumbleweed / Slowroll / Leap | zypper dependencies, Ninja build, `cmake --install /usr` |
| Alpine Linux | apk dependencies, Ninja build, `cmake --install /usr` |
| Void Linux | xbps dependencies, Ninja build, `cmake --install /usr` |
| Gentoo Linux | emerge dependencies, Ninja build, `cmake --install /usr` |
| Other Linux distributions | existing dependencies/toolchain, standard CMake install |

The dependency helper can be run by itself:

```sh
./scripts/install-dependencies.sh --dry-run
./scripts/install-dependencies.sh
```

It only uses repositories already configured on the system. It does not add PPAs, COPR repositories, OBS repositories or other third-party package sources. Quickshell packaging differs between distributions; if `quickshell` is not found after dependency installation, follow the upstream Quickshell 0.3+ installation guide before starting a LunaDash shell session.

Use `--skip-deps` when dependencies are already installed:

```sh
./scripts/install-session.sh --skip-deps
```

On non-Arch systems this is a direct CMake source installation rather than a distribution-owned package. Arch remains the package-managed path. The source build directory defaults to `build-install` and can be changed with `LUDASH_BUILD_DIR`.

If none of the recognized package managers is present, `install-dependencies.sh` enters generic validation mode. It verifies the build tools that it can detect and then lets CMake perform the authoritative Qt/Wayland dependency check instead of rejecting the distribution by name.

The installer never runs the desktop as root, stops the current desktop, reboots the computer, changes the default shell or modifies network services. `sudo` or `doas` is used only for package/system installation.

## Display manager and SDDM

If you already have an enabled display manager, keep it. Before logging out, run:

```sh
lunadash-session --check
```

Confirm `/usr/share/wayland-sessions/lunadash.desktop` exists and keep another desktop or TTY available. Log out, choose **LunaDash (Wayland)** in the existing session menu, then log in.

For a system without an enabled display manager, LunaDash can install and enable SDDM when systemd is present:

```sh
./scripts/install-session.sh --enable-sddm
```

This installs the distribution's `sddm` package where the installer knows the package-manager command, runs `systemctl enable sddm.service` without `--now`, and sets the next boot target to `graphical.target`. It refuses to replace a different enabled display manager. Save work and reboot yourself when ready; enabling SDDM never switches the current session.

On OpenRC/runit systems (common on Alpine, Void and some Gentoo installations), `--enable-sddm` intentionally refuses to proceed because service enablement is distribution-specific. Install LunaDash normally, then enable your display manager using that distribution's native service-management instructions.

For explicit passwordless login on boot, first verify normal SDDM login/logout, then use:

```sh
./scripts/install-session.sh --enable-sddm --autologin "$(id -un)"
```

Auto-login writes only `/etc/sddm.conf.d/90-ludash-autologin.conf` with `Session=lunadash.desktop` and `Relogin=false`; it refuses to overwrite an existing file. Do not enable it on a shared machine that requires a login barrier.

## Startup and configuration

The installed entry is `/usr/share/wayland-sessions/lunadash.desktop`. It calls `/usr/bin/lunadash-session`, which starts a private D-Bus session and the compositor using the wlroots DRM/KMS backend. After the Wayland socket exists, the compositor prepares an environment for Quickshell, optional Fcitx5 and applications. The session publishes display and input-method variables to its private D-Bus and, when available, the systemd user manager.

A nested compositor started directly from another desktop keeps the host activation environment untouched. Quickshell and applications connect to LunaDash through Wayland. The session uses the wlroots GLES2 renderer with a suitable DRM/KMS driver; both `LUDASH_GRAPHICS=opengl` and `gles` request that renderer. See [display and startup](DISPLAY_AND_STARTUP.md) for the loading animation, display modes and scaling. Package installation does not create GPU/input permissions; those come from PAM/logind/seat configuration. Do not work around permission failures by running LunaDash as root or making device nodes world-writable.

From a normal login, check installed commands and runtime paths:

```sh
lunadash-session --check
```

For a nested check from an existing Wayland desktop:

```sh
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE \
  QT_QPA_PLATFORM=wayland \
  lunadash-compositor --nested --socket ludash-test
```

Logs are owner-readable files under `${XDG_STATE_HOME:-$HOME/.local/state}/lunadash/session-*.log`. Inspect EGL/DRM/input errors there and the display-manager journal if login returns immediately.

The first launch displays a welcome message and a website help link. Dismiss it with **Start desktop**, then use Settings for language, network and appearance. Existing NetworkManager connections are reused. Session controls use logind D-Bus for supported suspend/reboot/poweroff actions and do not store passwords.

## Distribution notes

### Arch Linux

Arch is the primary development environment. `install-session.sh` creates the local source archive and installs the package through `makepkg`; removal therefore uses pacman:

```sh
sudo pacman -R ludash
```

### Debian / Ubuntu

The helper installs the Qt 6, Wayland, libinput, libxkbcommon, GL, udev/glib and build packages through apt. Ubuntu 24.04 is continuously compiled by the repository's main build workflow. A successful CI/source build does not imply physical GPU/login coverage on every Ubuntu or Debian release.

### Fedora

The helper uses Fedora's `qt6-qtbase-devel`, `qt6-qtdeclarative-devel`, `qt6-qtwayland-devel` and corresponding Wayland/input development packages, then follows the generic CMake install path.

### openSUSE

The helper uses Qt 6 development packages plus Wayland/input/GL development packages and follows the generic CMake install path. Tumbleweed is part of the distribution source-build workflow; older Leap releases can expose different Qt/package versions.

### Alpine Linux

The helper uses `apk` with `build-base`, Qt 6, Mesa, Wayland, libinput, libxkbcommon, eudev and GLib development packages. Alpine Edge is included in the distribution source-build workflow. Typical Alpine installations use OpenRC rather than systemd, so configure the display manager separately instead of using `--enable-sddm`.

### Void Linux

The helper uses `xbps-install`, including the Qt 6 development packages, Mesa/Wayland input stack and eudev development headers. The build/install path is supported by the installer, while CI coverage is currently narrower than Debian/Fedora/openSUSE/Alpine.

### Gentoo Linux

The helper uses `emerge --noreplace` with Qt 6 slots and the required Wayland/input/GL libraries. Existing USE flags still control how those packages are built; LunaDash does not rewrite Portage configuration. Gentoo currently uses the automatic installer path without the same container-CI breadth as the maintained binary-package distributions.

### Other Linux distributions

Install the dependencies listed in the [build and testing guide](TESTING_AND_FILES.md#build-and-static-checks), plus Quickshell 0.3+, then either run:

```sh
./scripts/install-session.sh --skip-deps
```

or build manually:

```sh
cmake -S . -B build-login -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-login --parallel
sudo cmake --install build-login
```

If your distribution uses a different display manager, keep it and select the installed `lunadash.desktop` Wayland session there.

## Recovery

Log out and choose the previous desktop. If optional auto-login prevents reaching the greeter, switch to another TTY and remove only the LunaDash auto-login drop-in:

```sh
sudo rm -- /etc/sddm.conf.d/90-ludash-autologin.conf
```

Then reboot when ready. If VT switching itself fails, use the distribution's recovery boot. SDDM is a separate package; keep it if other desktops use it. The installer does not delete or replace other desktops.

If you intentionally want console-only boot again on systemd, use `sudo systemctl set-default multi-user.target`; this changes the next boot target without stopping the current desktop.

## NVIDIA shell compatibility

LunaDash can select software rendering for Quickshell when the NVIDIA driver is loaded while retaining compositor GL/GLES effects. See [Shell rendering](SHELL_RENDERING.md) for overrides, resource testing and limitations.

Legacy `ludash-compositor`, `ludash-desktop`, `ludashctl`, `ludash-session`, and `ludash.desktop` names remain compatibility aliases. New integrations should use the LunaDash names. Installed shell QML lives under `/usr/share/lunadash/shell/`.

References: [Qt embedded Linux/EGLFS](https://doc.qt.io/qt-6/embedded-linux.html), [SDDM configuration](https://github.com/sddm/sddm/blob/develop/data/man/sddm.conf.rst.in).

### Discord and external application menus

LunaDash supports xdg-popup menus and nested submenus, including their initial configure, scene rendering and reposition requests. Pointer grabs remain with the menu until the client dismisses it. Focus changes precede the initiating button press, and clicks in an already focused window do not send redundant activation configures. Click focus resolves the actual parent surface, so applications with multiple windows do not always focus their first window.

Discord 1.0.157 can initialize an X11 input helper even when its main window uses native Wayland. The reported crash persisted with GPU rendering disabled: the captured crash was a null-display access in `XQueryExtension` on the native helper thread. Rendering flags alone cannot fix this failure. LunaDash's launcher now starts its authenticated XWayland service before Discord and passes `DISPLAY` and `XAUTHORITY` alongside `WAYLAND_DISPLAY`. The main window remains on Wayland. The helper's compatibility root stays hidden until an explicit X11 application is launched; it does not appear as an extra taskbar item. See [XWayland compatibility and limits](XWAYLAND.md).

Install XWayland (`xorg-xwayland` on Arch) and leave it enabled. The launcher reports a compatibility error if the helper server cannot start. After updating LunaDash, logging into the new session and completely exiting any existing Discord instance, launch from the application menu or use:

```sh
lunadashctl launch-command '["flatpak","run","com.discordapp.Discord","--enable-logging=stderr"]'
# Launcher and child diagnostics are written to the session log:
tail -f ~/.local/state/lunadash/session.log
```

A direct `flatpak run` command from an existing terminal bypasses server preparation and may have an unset or stale `DISPLAY`. Adding GPU flags to that command is not equivalent to launching through LunaDash. The Discord Flatpak must retain its X11 socket permission for the native helper; LunaDash does not rewrite personal Flatpak overrides.

The existing Discord-only software rendering fallback (`--disable-gpu`) remains while hardware compatibility is checked and may increase CPU use. `LUNADASH_DISCORD_GPU=1` in the session environment opts into ANGLE OpenGL with Vulkan features disabled. The session log records the selected rendering mode. Helper initialization tests do not establish that Discord networking, voice, screen sharing or physical NVIDIA rendering all work.

A missing FileChooser interface is a separate startup defect: the backend uses a local Qt theme to prevent circular portal activation, and the portal configuration uses GTK for other supported interfaces. Install `xdg-desktop-portal-gtk`; custom portal overrides take precedence. Fontconfig/theme warnings should be diagnosed separately. For persistent Flatpak options, see the [upstream Discord Flatpak instructions](https://github.com/flathub/com.discordapp.Discord#persistent-launch-options).

### Taskbar identity and selection

External window icons resolve from the Wayland application ID and the installed desktop entry's ID or startup class. Document titles remain visible in tooltips but cannot change an application's icon. Unknown or ambiguous identities use a generic icon. Task buttons retain the selected window ID through status polling and cancel a click if that slot was replaced. Selecting a window restores it if minimized, switches to its workspace and scrolls its tiled column into view. Group and member highlights follow the actual focused window.

### Window animations

Selecting an offscreen tiled window slides its column into view. Selecting an overlapping window uses a subtle fade, and new windows fade in with a short upward movement. Maximizing and restoring smoothly resize a retained frame before revealing the application's new layout; restoring also recovers the original column width. Closing a window shrinks and fades its last frame, including when the application closes itself.

Use **Settings → Appearance → Animations** and **Animation duration** to control motion. The default is 220 ms; turning animations off or setting the duration to zero applies changes immediately. Interrupted movement continues from the current visible position. The compositor requests new frames only while an effect is active, and resize transitions send the final size to the client once instead of resizing it on every animation frame. Preview images do not intercept pointer input and are released when the effect ends or its scene is destroyed.
