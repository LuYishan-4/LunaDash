# LunaDah

A C++20 Wayland tiling desktop with C11 rendering, geometry and metrics cores with an OpenGL / OpenGL ES compositor and a Quickshell interface. A Caelestia-inspired rounded panel, atmospheric wallpaper and a tabbed dashboard keep the workspace in view.

**0.1 development preview.** LunaDah is not a production-ready KDE replacement. Start with a nested session inside your existing desktop.

## What works

- Native Wayland windows with niri-inspired, one-window-per-column scrollable tiling, four workspaces, floating and minimized windows.
- Quickshell wallpaper and three-part panel: workspace/session controls on the left, a cropped BrandIcon launcher control in the center, and StatusNotifier tray icons plus compact status on the right.
- Saved accent colors, window gaps, panel height, wallpaper and information-card preferences.
- English and an external Traditional Chinese language pack.
- One ranked, token-searchable launcher list containing Settings, Files, Terminal, Monitor and installed desktop entries, with icon, name and description.
- Setting-level tokenized search, an expanded About page, and existing network connection detection with a NetworkManager configuration entry point.
- Files, command console, system monitor, pacman interface and opt-in metadata plugins.
- Configurable default backdrop blur, opacity and reduced-motion-friendly window/shell transitions.
- Optional X11 compatibility inside an authenticated XWayland window.
- Explicit OpenGL 3.3 compatibility / OpenGL ES 3.0 contexts and vertex/fragment shaders.

## Build on Arch Linux

```sh
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-wayland qt6-translations quickshell konsole fish mesa
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
QT_QPA_PLATFORM=wayland ./build/lunadah-compositor --socket ludash-test
```

For network configuration, optionally install `networkmanager nm-connection-editor`. LunaDah reuses existing connections; installing a package does not enable a network service. Avoid replacing an existing network manager without reviewing your distribution's configuration.

The first-run guide offers language, network and appearance settings. Offline use is supported. The launcher’s Settings entry reopens settings and the guide. Native application language changes take effect when those applications are reopened.

[Quickshell is packaged for Arch](https://archlinux.org/packages/extra/x86_64/quickshell/). The C++ backend needs Qt 6.4+, CMake 3.21+, C11/C++20 compilers and Khronos GL headers and Wayland development headers/scanner. The shell targets Quickshell 0.3; follow its [installation guide](https://quickshell.org/docs/v0.3.0/guide/install-setup/) on other distributions, where a newer Qt may be needed.

Ubuntu 24.04 backend packages: `build-essential cmake ninja-build pkg-config libwayland-dev qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland libqt6opengl6-dev`.
Fedora backend packages: `gcc-c++ cmake ninja-build pkgconf-pkg-config wayland-devel qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel`.

Arch, Ubuntu and Fedora builds are configured in CI. Configuration is not evidence that a remote job ran or that every distribution was verified. See the dated local results in the testing guide.

## Test and explore

```sh
sudo pacman -S --needed xorg-server-xvfb xorg-xauth xdotool python python-pillow
QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure
LUNADAH_DISABLE_FCITX=1 LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUNADAH_DISABLE_FCITX=1 LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

The integration script requires rendered client content, non-overlapping geometry and clean shutdown. An old success line followed by crashed processes is not a passing result.

| Shortcut | Action |
| --- | --- |
| Super + Enter / E / D | Default terminal / file manager / launcher |
| Super + J / K | Focus next / previous window |
| Super + H / L | Focus the column to the left / right |
| Super + Ctrl + H / L | Move the focused column left / right |
| Super + = / - | Widen / narrow the focused column |
| Super + C | Center the focused column |
| Super + 1–4 | Switch workspace |
| Super + Shift + 1–4 | Move focused window to workspace |
| Super + Space / F / M / Q | Toggle floating / fill / minimize / close |

Your host desktop may intercept Super shortcuts. The shell provides clickable workspace and launcher controls. The center BrandIcon opens a launcher that reveals downward from the panel center; the launcher has one application list, not separate categories or an open-window section.

## Installation and documentation

On Arch, `./scripts/install-session.sh` builds and installs a pacman-managed package and registers the login session. Use `--enable-sddm` only when you want SDDM enabled for the next boot; optional `--autologin USER` explicitly enables passwordless login. See [Boot and login session](docs/LOGIN_SESSION.md) for preflight, limitations and recovery.

For a staged install use `DESTDIR=/tmp/ludash-stage cmake --install build`. For a system install configure `-DCMAKE_INSTALL_PREFIX=/usr`, build, then run `sudo cmake --install build`. The Arch source package is created by `./scripts/make-source.sh`; run `makepkg -Cfs` in `packaging/arch`. The brand asset is installed at `/usr/share/lunadah/data/assets/icon.png`. The packaged EGLFS/KMS login session, physical SDDM login, and complete Fcitx behavior still need physical-session testing; no such verification is claimed.

- [Testing instructions and every maintained file](docs/TESTING_AND_FILES.md)
- [Traditional Chinese boot and login guide](docs/LOGIN_SESSION.zh-TW.md)
- [First-run setup and customization](docs/CONFIGURATION.md)
- [C core](docs/C_CORE.md), [blur and animations](docs/EFFECTS.md), [X11 compatibility](docs/XWAYLAND.md)
- [Appearance](docs/APPEARANCE.md) and [graphics contexts](docs/GRAPHICS.md)
- [Architecture](docs/ARCHITECTURE.md), [languages and input methods](docs/INPUT_METHODS.md)
- [Plugin development](docs/PLUGINS.md), [security and crash checks](docs/SECURITY_CHECKS.md)
- [Website and GitHub Pages deployment](docs/WEBSITE.md)

Missing or incomplete: multiple outputs, full layer-shell, screen locking, portals, notification hosting, native Wi-Fi credential UI, a polkit agent and complete input-method-v2 integration. StatusNotifier tray hosting is implemented; applications such as Fcitx, Discord and Docker appear when they export an SNI item. Native plugins are disabled by default and run without a sandbox when enabled. Pacman operations require a real terminal and retain sudo/pacman confirmation; this tool is unavailable on systems without pacman.

Licensed under GPL-3.0-only; see [LICENSE](LICENSE).

The dedicated settings center covers desktop preferences, workspaces, input, audio, power profiles and installed system tools. See [Settings coverage](docs/SETTINGS.md) for direct controls, host integrations and missing capabilities. The bundled Notes application has been removed.

Canonical installed commands are `lunadah-compositor`, `lunadah-desktop`, `lunadahctl`, and the extensionless `lunadah-session`; legacy `ludash-*` command names are compatibility aliases only. The normal application desktop ID is `lunadah-app.desktop`, installed shell QML is under `/usr/share/lunadah/shell/`, the BrandIcon source asset is `/usr/share/lunadah/data/assets/icon.png`, and session logs are under `~/.local/state/lunadah/` by default.

Shell blocks support validated JSON styles and optional trusted QML replacements. See [Module contract and templates](docs/MODULES.md) and [Default applications, Fish and Files](docs/DEFAULT_APPS_AND_FILES.md). LunaDah Files follows live desktop colors; the default interactive terminal is Konsole with a LunaDah Fish profile.

For NVIDIA descriptor exhaustion or Quickshell renderer overrides, see [Shell rendering](docs/SHELL_RENDERING.md). The NVIDIA default uses software Quickshell while retaining compositor GL/GLES effects.
