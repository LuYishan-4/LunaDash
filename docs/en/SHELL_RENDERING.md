# Shell rendering

Quickshell and the compositor are separate processes with independent rendering. The compositor uses wlroots. The shell uses Qt Quick. Renderer policy is applied only to the Quickshell child; other applications retain their own choices.

For a manual software-shell comparison in a nested session:

```sh
QT_QUICK_BACKEND=software LUDASH_TEST_HOST_WAYLAND=1 \
  LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
```

Software rendering can increase CPU usage, and custom GPU-only QML effects need a fallback. `shell/runtime/ShellRenderer` is applied when starting Quickshell. `LUDASH_SHELL_RENDERER` accepts `auto`, `opengl`, `vulkan` and `software`. Auto preserves explicit Qt renderer choices, otherwise selecting software on detected NVIDIA hosts and OpenGL elsewhere. This policy changes only the shell child environment; GTK and Qt application renderers remain their own choice.

Earlier NVIDIA/Qt observations motivated that compatibility helper and descriptor-soak tests. Those historical measurements are not evidence for the current wlroots renderer or every driver. Diagnose the current process and preserve its logs before making hardware claims. `QProcess: Cannot create pipe (Too many open files)` indicates resource exhaustion, not necessarily a missing executable.

## Built-in GLSL

The separate Qt render-element library embeds explicitly listed shaders from `src/compositor/renderer/opengl/shaders/`. `ShaderAssetLoader` recognizes vertex/fragment/geometry/compute/tessellation suffixes and aliases, including `.glsl`, `.glal` and `.shader` when a stage suffix or `#pragma ludash_stage` supplies the stage. The loader adds an appropriate GLSL version when absent; the active context must support it.

Renderer tests check embedded resource relocation, software OpenGL and failure cleanup. They do not prove absence of all GPU/fence leaks or that those retained Qt effects are connected to the wlroots scene. See [graphics](GRAPHICS.md), [effects](EFFECTS.md) and [testing](TESTING_AND_FILES.md).


## Shared surface design

The 1.0.1a shell uses one desktop design language for Dashboard, Settings, wallpaper pickers and portal wrappers: strong outer surfaces, glass cards, hairline borders, shared radii and bounded layouts. Desktop-visible plugins should consume host theme/context values instead of inventing unrelated window chrome.

Dashboard telemetry uses bounded layouts rather than absolute text placement. Settings supports a maximized presentation without changing page contracts. Desktop widget plugins render in the wallpaper Background surface.
