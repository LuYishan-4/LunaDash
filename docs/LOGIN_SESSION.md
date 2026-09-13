# Boot and login session

[Traditional Chinese guide](LOGIN_SESSION.zh-TW.md)

LuDash 0.1 is a development preview. The installer registers a real Wayland login entry, but the physical EGLFS/KMS GPU, input-seat and VT path has not been verified. Nested tests do not prove that a standalone login will work. Screen locking, complete portals, a polkit agent and multiple outputs remain incomplete. Keep a working desktop available during evaluation.

## Arch Linux installer

Run from the checkout as your normal user:

```sh
./scripts/install-session.sh --dry-run
./scripts/install-session.sh
```

The script installs build tools with pacman, creates the local source archive, then uses `makepkg --syncdeps --force --install`. Pacman owns the installed files. sudo and pacman retain their normal authorization prompts. The script never runs the desktop as root, stops the current desktop, reboots, changes the default shell or changes network services.

If you already have an enabled display manager, keep it. Log out, choose **LuDash (Wayland)** in its session menu, then log in. SDDM normally remembers the selected session. For a system without an enabled display manager, install and enable SDDM for the next boot:

```sh
./scripts/install-session.sh --enable-sddm
```

This runs `systemctl enable`, without `--now`, and sets the next boot target to `graphical.target`. It refuses to replace a different enabled display manager. Save work and reboot yourself when ready. Enabling SDDM does not switch the current session. SDDM's existing greeter configuration is preserved; its greeter backend is separate from the LuDash Wayland session.

For explicit passwordless login on boot, use the existing account name:

```sh
./scripts/install-session.sh --enable-sddm --autologin "$(id -un)"
```

Auto-login skips password authentication at boot. It writes only `/etc/sddm.conf.d/90-ludash-autologin.conf` with `Session=ludash.desktop` and `Relogin=false`; it refuses to overwrite that file. Review any existing `[Autologin]` settings in `/etc/sddm.conf` and other SDDM drop-ins, which may override it. First test manual login before enabling this option. Do not enable auto-login on a shared machine that needs a login barrier.

## Startup and configuration

The installed entry is `/usr/share/wayland-sessions/ludash.desktop`. It calls `/usr/bin/ludash-session`, which starts a private D-Bus session and the compositor using Qt EGLFS/KMS. Quickshell and applications connect to LuDash through Wayland. GLES 3 is the standalone default; `LUDASH_GRAPHICS=opengl` requests OpenGL 3.3 compatibility instead. A GPU driver and Qt's EGLFS/KMS platform integration are required. Package installation does not establish GPU/input permissions; these depend on the active PAM/logind seat and the Qt backend. Do not solve permission errors by running the desktop as root or making device nodes world-writable.

From a normal login, check installed commands and the runtime directory:

```sh
ludash-session --check
```

This does not acquire DRM devices or test display output. Do not start `ludash-session` inside another running desktop; use the login entry. For a nested check, run `QT_QPA_PLATFORM=wayland ludash-compositor --socket ludash-test` instead.

Logs are owner-readable files under `${XDG_STATE_HOME:-$HOME/.local/state}/ludash/session-*.log`. A new file is created for each login; remove old logs when no longer needed. Inspect EGL/DRM/input errors there and the display-manager journal if login returns immediately. `QT_QPA_EGLFS_INTEGRATION` can select a different installed Qt device integration for hardware that requires it; there is no universal vendor override.

The first-run guide provides language, offline/network configuration and appearance. The gear opens settings later. Existing network connections are reused. See [Settings](SETTINGS.md), [Modules](MODULES.md) and [Default apps](DEFAULT_APPS_AND_FILES.md) for customization.

## Recovery and removal

Log out and choose the previous desktop. If optional auto-login prevents reaching the greeter, switch to another TTY, sign in and remove only the LuDash auto-login drop-in:

```sh
sudo rm -- /etc/sddm.conf.d/90-ludash-autologin.conf
```

Then reboot when ready. If VT switching itself fails, use your distribution's recovery boot. To remove LuDash's package after logging into another desktop:

```sh
sudo pacman -R ludash
```

User preferences remain in the user's configuration directory. SDDM is a separate package; keep it if other desktops use it. The installer does not delete or replace those desktops.

If you intentionally want console-only boot again, use `sudo systemctl set-default multi-user.target`; this changes the next boot target without stopping the current desktop.

## Other Linux distributions

The convenience installer currently targets Arch and pacman-based systems. On other distributions install the dependencies from the README plus Quickshell, D-Bus, Konsole and Fish, then use CMake:

```sh
cmake -S . -B build-login -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-login --parallel 4
sudo cmake --install build-login
```

This is a direct source install, not a distribution-owned package. Use your distribution's display manager and session selection controls; no automatic apt/dnf service changes are performed. Physical session support remains unverified on those platforms.

References: [Qt embedded Linux/EGLFS](https://doc.qt.io/qt-6/embedded-linux.html), [SDDM configuration](https://github.com/sddm/sddm/blob/develop/data/man/sddm.conf.rst.in).

## NVIDIA shell compatibility

LuDash automatically selects software rendering for Quickshell when the NVIDIA driver is loaded, while retaining compositor GL/GLES effects. This avoids the observed sustained synchronization-descriptor growth on the tested host. See [Shell rendering](SHELL_RENDERING.md) for overrides, resource testing and limitations. Rebuild/reinstall and restart the session to apply changes.
