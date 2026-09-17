# Boot and login session

[Traditional Chinese guide](LOGIN_SESSION.zh-TW.md)

LunaDash is a development preview. The installer registers a real Wayland login entry, but nested testing does not prove that every physical EGLFS/KMS GPU, input-seat, VT or display-manager combination works. Keep another desktop or TTY available during evaluation.

## Supported installer paths

Run the installer from the checkout as your normal user:

```sh
./scripts/install-session.sh --dry-run
./scripts/install-session.sh
```

The same script now detects four distribution families:

| Distribution family | Build/install path |
| --- | --- |
| Arch Linux and derivatives | `makepkg --syncdeps --force --install`; pacman owns installed files |
| Debian / Ubuntu and derivatives | apt dependencies, Ninja build, `cmake --install /usr` |
| Fedora and derivatives | dnf dependencies, Ninja build, `cmake --install /usr` |
| openSUSE Tumbleweed / Slowroll | zypper dependencies, Ninja build, `cmake --install /usr` |

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

The installer never runs the desktop as root, stops the current desktop, reboots the computer, changes the default shell or modifies network services. `sudo` or `doas` is used only for package/system installation.

## Display manager and SDDM

If you already have an enabled display manager, keep it. Before logging out, run:

```sh
lunadash-session --check
```

Confirm `/usr/share/wayland-sessions/lunadash.desktop` exists and keep another desktop or TTY available. Log out, choose **LunaDash (Wayland)** in the existing session menu, then log in.

For a system without an enabled display manager, LunaDash can install and enable SDDM on pacman, apt, dnf and zypper systems:

```sh
./scripts/install-session.sh --enable-sddm
```

This installs the distribution's `sddm` package, runs `systemctl enable sddm.service` without `--now`, and sets the next boot target to `graphical.target`. It refuses to replace a different enabled display manager. Save work and reboot yourself when ready; enabling SDDM never switches the current session.

For explicit passwordless login on boot, first verify normal SDDM login/logout, then use:

```sh
./scripts/install-session.sh --enable-sddm --autologin "$(id -un)"
```

Auto-login writes only `/etc/sddm.conf.d/90-ludash-autologin.conf` with `Session=lunadash.desktop` and `Relogin=false`; it refuses to overwrite an existing file. Do not enable it on a shared machine that requires a login barrier.

## Startup and configuration

The installed entry is `/usr/share/wayland-sessions/lunadash.desktop`. It calls `/usr/bin/lunadash-session`, which starts a private D-Bus session and the compositor using Qt EGLFS/KMS. After the Wayland socket exists, the compositor prepares an environment for Quickshell, optional Fcitx5 and applications. The session publishes display and input-method variables to its private D-Bus and, when available, the systemd user manager.

A nested compositor started directly from another desktop keeps the host activation environment untouched. Quickshell and applications connect to LunaDash through Wayland. GLES 3 is the standalone default; `LUDASH_GRAPHICS=opengl` requests OpenGL 3.3 compatibility instead. A suitable GPU driver and Qt EGLFS/KMS platform integration are required. Package installation does not create GPU/input permissions; those come from PAM/logind/seat configuration. Do not work around permission failures by running LunaDash as root or making device nodes world-writable.

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

The first-run guide provides language, network/offline setup and appearance. Existing NetworkManager connections are reused. Session controls use logind D-Bus for supported suspend/reboot/poweroff actions and do not store passwords.

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

### openSUSE Tumbleweed / Slowroll

The helper uses the `libqt6-*` development package names plus Wayland/input/GL development packages and follows the generic CMake install path. Leap releases may expose a different Qt/package set and are not included in the automatic support promise.

### Other Linux distributions

Install equivalent requirements from the README, Quickshell 0.3+, then build manually:

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

If you intentionally want console-only boot again, use `sudo systemctl set-default multi-user.target`; this changes the next boot target without stopping the current desktop.

## NVIDIA shell compatibility

LunaDash can select software rendering for Quickshell when the NVIDIA driver is loaded while retaining compositor GL/GLES effects. See [Shell rendering](SHELL_RENDERING.md) for overrides, resource testing and limitations.

Legacy `ludash-compositor`, `ludash-desktop`, `ludashctl`, `ludash-session`, and `ludash.desktop` names remain compatibility aliases. New integrations should use the LunaDash names. Installed shell QML lives under `/usr/share/lunadash/shell/`.

References: [Qt embedded Linux/EGLFS](https://doc.qt.io/qt-6/embedded-linux.html), [SDDM configuration](https://github.com/sddm/sddm/blob/develop/data/man/sddm.conf.rst.in).
