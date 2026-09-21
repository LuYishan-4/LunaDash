# Source architecture

LunaDash is a development Wayland desktop built with C++20, C11, wlroots and Qt 6. Quickshell/QML owns the desktop shell. Qt Widgets implements the native tools; Qt Wayland is used by clients, not as the compositor server. Arch Linux is the primary development platform. Passing nested or software-rendered tests does not establish complete hardware or login-session support.

## Source domains

Headers and implementations are colocated. Directories use lowercase domain names, C++ files use PascalCase `.hpp` / `.cpp`, and C interfaces retain `.h` / `.c`. Project C++ uses `LunaDash`; C symbols keep their `ludash_` prefix and existing C type names. Settings keys, resource IDs for translations, executable aliases and protocol names retain their existing compatibility spellings.

```text
src/
  compositor/
    animation/       wlroots scene transitions and retained Qt adapters
    client/          compositor-owned client state
    input/           seat events, keyboard maps and input-method bridge
    ipc/             bounded local JSON control socket
    plugins/         native effect loading and versioned interface
    renderer/        rendering orchestration and render elements
      opengl/        C GL dispatch, context, shader/program/texture/FBO ownership
        shaders/     explicitly embedded built-in GLSL
      blur/          Qt Quick item and geometry adapter
      wallpaper/     Qt Quick wallpaper item
    session/         compositor startup, child environment and launch policy
    layout/          layout contract and strategy factory
    stacking/        persistent overlapping-window strategy for plugins
    tiling/          bounded split tree, grouped rows and C geometry
    wayland/         runtime, output and surface lifetimes
      wlroots/       version compatibility and external C header boundaries
    window/          rules and retained Qt Quick window adapters
    xwayland/        authenticated, on-demand X11 compatibility container
    Main.cpp
  config/            command runner, preferences, localization, plugin catalog
  core/              reusable contracts, listener helpers and build definitions
  ctl/               control client and Main.cpp
  desktop/           data/actions exposed to shell and native tools
  service/portal/    file chooser D-Bus service and Main.cpp
  shell/             generic module schema/runtime, media tool and Main.cpp
```

Each `Main.cpp` delegates to a module implementing `run(argc, argv)`. `SessionApplication`, `DesktopApplication`, `ControlClient`, `ShellTool` and `Portal` own their startup and event loop. The portal's file chooser implementation is separate from D-Bus service registration. `JsonTranslator` is separate from dictionary loading. Shell module configuration belongs to `shell/modules`; metrics, status and update checking belong to `desktop/system`.

## Dependency and ownership rules

Dependencies flow from `core` to `config` to runtime domains. Core cannot include any higher domain. Configuration cannot include compositor or desktop services. Desktop code cannot include compositor internals. Renderer code cannot depend on desktop UI. The source-root include style is uniform, for example `"compositor/renderer/Renderer.hpp"`; deep relative includes are rejected.

`WaylandCompositor` owns desktop/session integration. Its private `Runtime.hpp` declares wlroots state; `Runtime.cpp` owns setup/teardown, `Output.cpp` owns output state, `Surface.cpp` owns xdg/layer surfaces, and `input/Input.cpp` owns input event and input-method handling. `input/Keyboard.cpp` applies physical keyboard maps; `desktop/input` validates preferences without including wlroots. Version-dependent xdg role events and destruction preserve the remote implementation's wlroots 0.17/0.18+ distinctions.

`core/templates/WaylandListener.hpp` centralizes listener registration and detachment. Listeners disconnect before their wlroots owner is destroyed. Window close transitions retain scene snapshots without retaining dead surface callbacks. `SceneWindowAnimations` remains the active wlroots animation controller. Qt scene-graph adapters are compiled separately and must not be mistaken for active wlroots effects.

Plugin metadata discovery is configuration (`config/plugins/PluginCatalog`), so desktop settings can inspect plugins without depending on the compositor loader. Native effects remain disabled by default and require explicit user enablement. Metadata is not a sandbox. Rebuild native plugins against the current installed header after this source namespace change; a stable native binary ABI is not promised for development builds.

## Rendering

The active compositor uses wlroots renderer/allocator/scene ownership. Its `--graphics opengl` and `--graphics gles` preferences both request wlroots' GLES2 renderer; wlroots can select another available renderer in automatic mode. Headless CI explicitly selects pixman. These are distinct from the retained Qt OpenGL render-element library.

The render-element library has one `Renderer`, one compile-time `ActiveGraphics` selection in `RendererConfig.hpp`, and an `OpenGL` implementation. `RendererTypes.hpp` carries common state without exposing GL handles. `Feature`, `Module` and `RendererTemplate` provide small, consumed lifecycle contracts rather than a global singleton framework. Only create additional provider/manager abstractions when there is an actual extension boundary.

All project-owned raw OpenGL implementation lives in `renderer/opengl`, including blur, wallpaper and decoration passes. `GLDispatch.c` is the C11 function-loading core. `Shader`, `Program`, `Texture` and `Framebuffer` own their GL resources. Failed shader compilation, later-stage creation failure and linking failure release previously allocated objects. Qt owns the active context; render resources must be destroyed on its render thread while that context is current.

Built-in shaders are listed explicitly in `cmake/modules/Renderer.cmake` and embedded at `:/LunaDash/renderer/shaders/`. They load from the same IDs after installation or relocation. `data/` contains application assets, module templates, translations and plugin/portal metadata, not internal GLSL. SDK 2 plugin-provided shaders are a separate, explicitly trusted resource class, baked by the plugin CMake SDK for Quickshell visual slots.

## Extending a feature

Choose an existing domain, add a cohesive interface/implementation pair, identify the lifetime owner, and list each source in the owning CMake target. Add shared types only when needed. Keep QML under `qml/<feature>/`, translation strings in external catalogs, and platform switches near implementation boundaries. The public compile definitions use `LUDASH_*`; `core/Defines.hpp` and `config/BuildConfig.hpp` expose common capabilities and version metadata. Native rendering selection is centralized rather than repeated throughout feature code.

Reusable module contracts use composition and C++20 concepts. Runtime virtual dispatch is appropriate for existing render elements and the native plugin boundary. Do not introduce an inheritance hierarchy, empty directory or singleton merely to match a diagram.

## Enforcement and limitations

Run `python3 scripts/check-source-layout.py`. It rejects incorrect source naming, old renderer names, duplicate render trees, forbidden include directions, missing explicit CMake sources, shaders outside their owner and raw OpenGL implementation outside its module. `tests/architecture/test_source_layout.py` verifies rejection behavior. Existing source-style and Ubuntu workflows run these checks.

The renderer regression tests load embedded assets from an unrelated directory, report missing-context errors, inject GL failures to check cleanup, and optionally compile/draw through a software OpenGL context after a staged install. These checks do not establish physical GPU correctness.

Wayland CI exercises protocol globals, xdg-toplevel commit/map/unmap/destroy, client startup, on-demand XWayland and clean shutdown. Chrome, Zed, Fcitx candidate placement, physical input, multiple physical outputs and display-manager login still need application/hardware testing. The current file chooser portal is not a complete screen-sharing/PipeWire portal. There is no complete screen-locking or polkit-agent implementation. See [testing](TESTING_AND_FILES.md), [graphics](GRAPHICS.md), [input methods](INPUT_METHODS.md) and [release process](RELEASE_PROCESS.md).

Media discovery and control are owned by `src/shell/media/`, with QML presentation under `qml/overview/`. See [Media](MEDIA.md) and [Window layout templates](WINDOW_LAYOUT_TEMPLATES.md).
