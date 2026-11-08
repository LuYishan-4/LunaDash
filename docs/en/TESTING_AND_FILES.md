# Build and source map

Finish code, packaging and documentation changes before building. Arch Linux is the primary development environment; CI configuration is not a substitute for observing a completed run on the intended commit.

Clone the `dev` branch with `git clone --branch dev ...`. The normal install command downloads the Shell revision recorded in `packaging/shell-revision`; website workflows fetch LunaDash-docs separately into a temporary directory. No recursive checkout is needed. See the [repository workflow](COMPONENT_REPOS.md) for editing and submitting changes.

## Build and static checks

The toolchain requires CMake 3.21+, Ninja, Python 3, C11/C++20, Rust/Cargo 1.92+ for pinned Slint 1.18.1, Qt 6.4+ Core/Gui/Widgets/Quick/OpenGL/Concurrent/Network/DBus for the remaining native targets, wlroots 0.17–0.20, Wayland protocols/scanner, xkbcommon, pixman, libdrm, GL headers, GLib and Fontconfig development files. The standalone Welcome target builds Slint's Winit Wayland backend with FemtoVG and explicitly disables Slint's Qt and X11 backends. `lunadashctl` uses POSIX Unix sockets and nlohmann/json; CMake uses an installed nlohmann_json 3.11+ package or fetches pinned v3.11.3. `scripts/install-dependencies.sh` provides distribution-specific package selection. The shell runtime, SlintLu, is built from source as well (also Rust); it links Wayland, xkbcommon, Fontconfig, FreeType and PAM. The default terminal is Kitty. XWayland provides optional X11 compatibility. Interactive capture uses grim and slurp; backlight controls use brightnessctl and external-monitor controls use ddcutil.

```sh
python3 scripts/check-source-layout.py
python3 scripts/check-translations.py
python3 scripts/ci/check_shell_actions.py
python3 scripts/gen-ui-icons.py --check
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

The architecture checker enforces lowercase domains, PascalCase C++ filenames, small domain-local entrypoints, source-root includes, dependency direction, raw GL placement and explicit CMake inventories. `.clang-format` defines source formatting; use `clang-format -i` on modified C/C++ files. See [CI](CI.md) for what CI runs.

## Runtime and installation

```sh
DESTDIR="$PWD/build/stage" cmake --install build --prefix /usr
```

Normal installation includes executables/compatibility aliases, session entries, shell QML, translations, portal configuration, assets and the optional example plugin. When `BUILD_SHARED_LIBS` is enabled, LunaDash's internal shared libraries are installed in `CMAKE_INSTALL_LIBDIR` and executables use relative runtime paths; static builds need no separate library installation. Internal GLSL is embedded and needs no installed shader directory.

On Arch, also verify the source archive used by `./scripts/install-session.sh`, since a checkout build cannot detect missing archive contents:

```sh
./scripts/make-source.sh
(cd packaging/arch && makepkg --cleanbuild --force --nosign)
```

This builds packages without installing them. The archive must include `data/plugins/` for native plugin examples and `templates/` for the installed plugin SDK, in addition to the main source tree. Inspect the resulting package for the SDK templates and plugin examples before publishing it.

## CI coverage

[Dev CI and Main CI](CI.md) only build: Ubuntu, the website, the Nix package and Arch (all five distributions on main and full runs). No workflow runs tests; a green run proves only that the commit builds. Inspect completed results for the exact commit.

[NixOS](NIXOS.md) has a locked flake, package and system module; CI only builds the x86_64 package. Void/Gentoo retain installer paths without the same CI coverage.

A successful build does not verify runtime behavior. A complete manual session checks Chrome/Zed, pointer and physical keyboard input, Fcitx preedit/candidate positioning, wallpaper changes, animation, file chooser D-Bus activation and logout. Full settings-page coverage, region selection, window motion, hardware brightness and application compatibility also require real-session verification when preparing a release.

## Source map

| Location | What to edit |
| --- | --- |
| `src/compositor/session` | Startup, child environments and launch policy |
| `src/compositor/wayland` | Runtime, output and surface lifetimes; private wlroots compatibility boundary |
| `src/compositor/input` | Physical keyboard and pointer handling, text-input/input-method bridge |
| `src/compositor/tiling`, `window`, `client` | Layout, rules and client state |
| `src/compositor/window/animation` | wlroots scene transitions |
| `src/compositor/renderer` | Render orchestration, common types and item adapters |
| `src/compositor/renderer/opengl` | GL dispatch/resources/passes and embedded shaders |
| `src/config` | Preferences, localization, process helper and plugin metadata |
| `src/core` | Shared lifecycle concepts, listener template and compile definitions |
| `src/desktop` | Applications, external default-app policy, audio, network, power and system services |
| `src/service/portal` | D-Bus startup and native file chooser implementation |
| `src/ctl` | Bounded local control client |
| `nix`, `flake.nix`, `flake.lock` | Nix package, NixOS module and pinned dependency input |
| LunaDash-Shell | Shell feature UI and shared controls, installed as a separate source repository |
| LunaDash-docs | Astro website source, fetched by its own CI and the root Pages workflow |
| `src/compositor` | Compositor sources, built with the root CMake targets |
| `data` | User-facing assets, templates, translations and portal/plugin metadata |
| `cmake/modules/Renderer.cmake` | Explicit render sources and embedded shader inventory |
| `scripts` | Installation, diagnostics and repository checks |
| `LunaDash-docs/src/pages/docs/architecture.astro` | Website architecture guide in LunaDash-docs |

See [architecture](ARCHITECTURE.md) for ownership contracts.
