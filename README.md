# LuDash

A C++20 Wayland tiling desktop with an OpenGL / OpenGL ES compositor and a Quickshell interface. A quiet, segmented panel, atmospheric wallpaper and a translucent information card keep the workspace in view.

**0.1 development preview.** LuDash is not a production-ready KDE replacement. Start with a nested session inside your existing desktop.

## What works

- Native Wayland windows, master/stack tiling, four workspaces, floating and minimized windows.
- Quickshell wallpaper, panel, launcher, settings, first-run guide and logout confirmation.
- Saved accent colors, window gaps, panel height, wallpaper and information-card preferences.
- English and an external Traditional Chinese language pack.
- Existing network connection detection and a NetworkManager configuration entry point.
- Files, notes, command console, system monitor, pacman interface and opt-in metadata plugins.
- Explicit OpenGL 3.3 Core / OpenGL ES 3.0 contexts and vertex/fragment shaders.

## Build on Arch Linux

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-wayland qt6-translations quickshell mesa
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
QT_QPA_PLATFORM=wayland ./build/ludash-compositor --socket ludash-test
```

For network configuration, optionally install `networkmanager nm-connection-editor`. LuDash reuses existing connections; installing a package does not enable a network service. Avoid replacing an existing network manager without reviewing your distribution's configuration.

The first-run guide offers language, network and appearance settings. Offline use is supported. The gear reopens settings and the guide. Native application language changes take effect when those applications are reopened.

[Quickshell is packaged for Arch](https://archlinux.org/packages/extra/x86_64/quickshell/). The C++ backend needs Qt 6.4+, CMake 3.21+, a C++20 compiler and Wayland development headers/scanner. The shell targets Quickshell 0.3; follow its [installation guide](https://quickshell.org/docs/v0.3.0/guide/install-setup/) on other distributions, where a newer Qt may be needed.

Ubuntu 24.04 backend packages: `build-essential cmake ninja-build pkg-config libwayland-dev qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland libqt6opengl6-dev`.
Fedora backend packages: `gcc-c++ cmake ninja-build pkgconf-pkg-config wayland-devel qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel`.

Arch, Ubuntu and Fedora builds are configured in CI. Configuration is not evidence that a remote job ran or that every distribution was verified. See the dated local results in the testing guide.

## Test and explore

```sh
sudo pacman -S --needed xorg-server-xvfb xorg-xauth xdotool python python-pillow
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

The integration script requires rendered client content, non-overlapping geometry and clean shutdown. An old success line followed by crashed processes is not a passing result.

| Shortcut | Action |
| --- | --- |
| Super + Enter / E / D | Console / files / launcher |
| Super + J / K | Focus next / previous window |
| Super + H / L | Change master-column ratio |
| Super + 1–4 | Switch workspace |
| Super + Shift + 1–4 | Move focused window to workspace |
| Super + Space / F / M / Q | Toggle floating / fill / minimize / close |

Your host desktop may intercept Super shortcuts. The shell provides clickable workspace and launcher controls.

## Installation and documentation

For a staged install use `DESTDIR=/tmp/ludash-stage cmake --install build`. For a system install configure `-DCMAKE_INSTALL_PREFIX=/usr`, build, then run `sudo cmake --install build`. The Arch source package is created by `./scripts/make-source.sh`; run `makepkg -Cfs` in `packaging/arch`. The packaged EGLFS/KMS login session still needs physical GPU, seat and VT testing.

- [Testing instructions and every maintained file](docs/TESTING_AND_FILES.md)
- [First-run setup and customization](docs/CONFIGURATION.md)
- [Appearance](docs/APPEARANCE.md) and [graphics contexts](docs/GRAPHICS.md)
- [Architecture](docs/ARCHITECTURE.md), [languages and input methods](docs/INPUT_METHODS.md)
- [Plugin development](docs/PLUGINS.md), [security and crash checks](docs/SECURITY_CHECKS.md)
- [Website and GitHub Pages deployment](docs/WEBSITE.md)

Missing or incomplete: XWayland, multiple outputs, full layer-shell, screen locking, portals, audio controls, full system tray, native Wi-Fi credential UI, a polkit agent and complete input-method-v2 integration. Native plugins are disabled by default and run without a sandbox when enabled. Pacman operations require a real terminal and retain sudo/pacman confirmation; this tool is unavailable on systems without pacman.

Licensed under GPL-3.0-only; see [LICENSE](LICENSE).
