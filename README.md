<div align="center">

<a href="https://github.com/LuYishan-4/LunaDash">
  <img src="docs/brand/banner.svg" alt="LunaDash — a moonlit, focused Linux desktop" width="880">
</a>

<br>

### A moonlit, focused Linux desktop

<p>
  <img src="https://img.shields.io/badge/license-GPL--3.0--only-9ccbfb?style=flat-square" alt="GPL-3.0-only">
  <img src="https://img.shields.io/badge/Linux-Arch%20%C2%B7%20Debian%2FUbuntu%20%C2%B7%20Fedora%20%C2%B7%20openSUSE-9ccbfb?style=flat-square" alt="Arch, Debian Ubuntu, Fedora and openSUSE">
  <img src="https://img.shields.io/badge/stack-C%2B%2B20%20%C2%B7%20Qt%206%20%C2%B7%20Wayland-9ccbfb?style=flat-square" alt="C++20, Qt 6, Wayland">
  <img src="https://img.shields.io/badge/shell-Quickshell%20QML-9ccbfb?style=flat-square" alt="Quickshell QML shell">
  <a href="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-gate.yml">
    <img src="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-gate.yml/badge.svg" alt="Main required checks">
  </a>
</p>

</div>

LunaDash is a Wayland desktop: a C++20 / OpenGL compositor built on Qt Wayland Compositor, C11 cores for rendering, tiling and metrics, and a Quickshell/QML shell. Windows live in niri-inspired scrollable columns, while the shell provides the panel, launcher, dashboard, settings, notifications, file tools and desktop integrations.

> [!WARNING]
> LunaDash is still in active pre-release development. Nested sessions are the safest way to test it. Keep your existing desktop/login session available as a recovery path.

## Screenshots

<table align="center">
  <tr>
    <td><img src="docs/image/1.png" alt="LunaDash screenshot 1" width="420"></td>
    <td><img src="docs/image/2.png" alt="LunaDash screenshot 2" width="420"></td>
  </tr>
  <tr>
    <td><img src="docs/image/5.png" alt="LunaDash screenshot 3" width="420"></td>
    <td><img src="docs/image/6.png" alt="LunaDash screenshot 4" width="420"></td>
  </tr>
</table>

## Highlights

- **Scrollable columns and workspaces.** Tiled windows live in horizontally scrollable columns, can be grouped, resized, floated and moved between workspaces.
- **LunaDash panel and launcher.** A theme-aware top panel provides workspaces, running applications, the Luna launcher, system tray, USB devices, date/time and session controls.
- **Dashboard.** Quick volume and Wi-Fi controls, launch shortcuts and MPRIS media controls with current album art / artwork preview for compatible players such as Spotify, browser media and VLC.
- **Settings centre.** Appearance, windows, shortcuts, modules, dashboard, displays, input/Fcitx, sound, network, Bluetooth, devices/disks, power, privacy, system, applications, plugins and updates share the same QML design system.
- **Files and portals.** LunaDash Files supports file operations, context menus, Open With/default file associations and terminal integration; the desktop also includes a file chooser portal backend.
- **Notifications and removable media.** Desktop notifications, crash details and removable USB notifications/operations are integrated into the shell.
- **Plugins.** Metadata-driven QML plugins and native C++ effects use a common manifest model; native effects remain opt-in because they execute in-process.
- **X11 compatibility.** X11 applications run through an authenticated XWayland instance.
- **Explicit graphics.** OpenGL 3.3 compatibility or OpenGL ES 3.0 contexts with a software-friendly shell rendering path for problematic drivers.

## Linux distribution support

LunaDash uses standard CMake install rules and is no longer tied to pacman. The installer currently recognizes these package-manager families:

| Distribution family | Installation path | Status |
| --- | --- | --- |
| Arch Linux and derivatives | `makepkg` + pacman package | Primary development path |
| Debian / Ubuntu and derivatives | apt dependencies + CMake install | Supported build/install path |
| Fedora and derivatives | dnf dependencies + CMake install | Supported build/install path |
| openSUSE Tumbleweed / Slowroll | zypper dependencies + CMake install | Supported build/install path |
| Other Linux distributions | Manual dependencies + standard CMake | Expected to work when requirements are available; not automatically provisioned |

The backend requires CMake 3.21+, Ninja, a C11/C++20 compiler, Qt 6.4+ Base/Declarative/Wayland/OpenGL development packages, Wayland/wayland-protocols, libinput, libxkbcommon, udev/systemd development headers, GL development headers, GLib and shared-mime-info. The shell requires [Quickshell 0.3 or newer](https://quickshell.org/docs/v0.3.0/guide/install-setup/). Quickshell packaging differs between distributions, so LunaDash does **not** add an unofficial repository automatically.

### Install dependencies only

```sh
./scripts/install-dependencies.sh
```

That script detects `pacman`, `apt-get`, `dnf` or `zypper` and installs dependencies only from repositories already enabled on the system. Use `--dry-run` to inspect commands first.

Typical package sets are handled automatically. For reference:

```sh
# Arch Linux
sudo pacman -S --needed base-devel cmake ninja git pkgconf libglvnd mesa \
  wayland wayland-protocols libinput libxkbcommon systemd glib2 \
  qt6-base qt6-declarative qt6-wayland qt6-translations shared-mime-info fish

# Debian / Ubuntu
sudo apt-get install build-essential cmake ninja-build git pkg-config libgl-dev \
  libwayland-dev wayland-protocols libinput-dev libxkbcommon-dev libudev-dev \
  libglib2.0-dev qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland \
  libqt6opengl6-dev shared-mime-info fish

# Fedora
sudo dnf install gcc gcc-c++ cmake ninja-build git pkgconf-pkg-config \
  mesa-libGL-devel wayland-devel wayland-protocols-devel libinput-devel \
  libxkbcommon-devel systemd-devel glib2-devel qt6-qtbase-devel \
  qt6-qtdeclarative-devel qt6-qtwayland-devel shared-mime-info fish

# openSUSE
sudo zypper install gcc gcc-c++ cmake ninja git pkg-config Mesa-libGL-devel \
  wayland-devel wayland-protocols-devel libinput-devel libxkbcommon-devel \
  systemd-devel glib2-devel libqt6-qtbase-devel libqt6-qtdeclarative-devel \
  libqt6-qtwayland-devel shared-mime-info fish
```

## Install LunaDash

```sh
git clone https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
./scripts/install-session.sh
```

`install-session.sh` now chooses the distribution path automatically. Arch creates and installs the local package from `packaging/arch`; Debian/Ubuntu, Fedora and openSUSE build with Ninja and use `sudo cmake --install` under `/usr`. `sudo` or `doas` is used only for package/system installation.

Useful options:

```sh
./scripts/install-session.sh --dry-run
./scripts/install-session.sh --skip-deps
./scripts/install-session.sh --enable-sddm
./scripts/install-session.sh --enable-sddm --autologin USER
```

The installer never replaces an already-enabled different display manager. `--enable-sddm` is explicit, and auto-login is a separate opt-in. If Quickshell is not available from the distribution, the installer completes the compositor installation and tells you to install Quickshell separately.

### Manual installation on another Linux distribution

If your distribution is not detected, install the requirements above and use the standard build path:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
sudo cmake --install build
```

Then ensure Quickshell 0.3+ is in `PATH`. The canonical executables are `lunadash-compositor`, `lunadash-desktop`, `lunadashctl` and `lunadash-session`; older `ludash-*` names remain compatibility aliases.

## Test without replacing your desktop

A nested session is recommended before trying a login session:

```sh
cmake -S . -B build -G Ninja
cmake --build build
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE \
  QT_QPA_PLATFORM=wayland ./build/lunadash-compositor --nested --socket ludash-test
```

You can also run the project integration helper:

```sh
./scripts/test-once.sh
```

A passing software/nested session validates that path only; it is not evidence that every GPU, display manager or physical seat works.

## Keyboard shortcuts

`Super` means the Meta key. Bindings can be changed or disabled in **Settings → Keyboard shortcuts**.

| Shortcut | Action |
| --- | --- |
| `Alt` + `Shift` + `F5` | Take a screenshot |
| `Super` + `Return` / `E` / `D` | Default terminal / Files / launcher |
| `Super` + `H` / `L` | Focus the column to the left / right |
| `Super` + `K` / `J` | Focus the previous / next member in the column |
| `Super` + `Shift` + `H` / `L` | Merge the focused window into the left / right column |
| `Super` + `Shift` + `E` | Expel the focused member into its own column |
| `Super` + `Ctrl` + `H` / `L` | Move the focused column left / right |
| `Super` + `=` / `-` | Widen / narrow the focused column |
| `Super` + `Shift` + `C` | Center the focused column |
| `Super` + `F` | Maximize or restore the focused window |
| `Super` + `C` / `Q` | Close the focused window |
| `Super` + `M` | Minimize the focused window |
| `Super` + `Space` | Toggle floating |
| `Super` + `1`–`9` | Switch workspace |
| `Super` + `Shift` + `1`–`9` | Move the focused window to a workspace |

The host desktop may intercept `Super` shortcuts while LunaDash is running nested.

## Settings and control socket

`lunadashctl` talks to the running compositor, and the settings UI uses the same control path:

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl open-settings appearance
./build/lunadashctl appearance '{"gap":16,"panelHeight":44}'
./build/lunadashctl check-update
./build/lunadashctl screenshot
```

## Contributing and releases

All pull requests must target **`dev`**. PRs are expected to update both relevant files under `docs/` and the user-facing website under `site/`; the PR policy workflow enforces this. Heavy C++/Qt/security analysis is scoped to source changes instead of running unnecessarily for documentation-only changes.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the PR contract and [Release process](docs/RELEASE_PROCESS.md) for releases. GitHub Release Markdown is the canonical release-note source; release publication synchronizes it into the website release-notes section.

## Documentation

- [Testing guide and maintained files](docs/TESTING_AND_FILES.md)
- [Architecture](docs/ARCHITECTURE.md) · [C core](docs/C_CORE.md) · [Graphics contexts](docs/GRAPHICS.md)
- [Appearance](docs/APPEARANCE.md) · [Effects](docs/EFFECTS.md) · [Shell rendering](docs/SHELL_RENDERING.md)
- [Configuration](docs/CONFIGURATION.md) · [Settings](docs/SETTINGS.md) · [Modules](docs/MODULES.md)
- [Default applications and Files](docs/DEFAULT_APPS_AND_FILES.md) · [Input methods](docs/INPUT_METHODS.md)
- [Plugins](docs/PLUGINS.md) · [Security checks](docs/SECURITY_CHECKS.md)
- [Boot/login session](docs/LOGIN_SESSION.md) · [Website](docs/WEBSITE.md) · [Release process](docs/RELEASE_PROCESS.md)

## Project status

LunaDash is a development desktop, not a claim of universal hardware compatibility. Arch Linux remains the primary development environment. Ubuntu is continuously used by the repository build/runtime workflows; Debian/Ubuntu, Fedora and openSUSE now have maintained dependency/install paths, but physical-session coverage varies by distribution and hardware.

Keep an existing desktop/session available when testing standalone login. Native plugins execute without a sandbox when enabled. Hardware-specific features such as GPU/DRM behaviour, multiple outputs and input-method integration can still vary between systems.

## License

GPL-3.0-only. See [LICENSE](LICENSE).
