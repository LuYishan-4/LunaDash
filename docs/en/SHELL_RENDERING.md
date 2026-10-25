# Shell rendering

The shell and the compositor are separate processes with independent rendering. The compositor uses wlroots. The Slint shell runs on SlintLu, which renders with OpenGL, Vulkan or software and falls back on its own. Renderer policy is applied only to the shell child; other applications retain their own choices.

For a manual software-shell comparison in a nested session:

```sh
LUDASH_SHELL_RENDERER=software LUDASH_TEST_HOST_WAYLAND=1 \
  LUDASH_BUILD_DIR="$PWD/build" ./scripts/test-wayland.sh
```

Software rendering can increase CPU usage. `shell/runtime/ShellRenderer` is applied when starting the shell. `LUDASH_SHELL_RENDERER` accepts `auto`, `opengl`, `vulkan` and `software`. For the Slint shell it becomes `SLINTLU_RENDERER`; `auto` keeps an explicit `SLINTLU_RENDERER` and otherwise lets SlintLu choose. A legacy Quickshell shell keeps the former policy: explicit Qt renderer choices win, otherwise software on detected NVIDIA hosts and OpenGL elsewhere. This policy changes only the shell child environment; GTK and Qt application renderers remain their own choice.

Earlier NVIDIA/Qt observations motivated that compatibility helper and descriptor-soak tests. Those historical measurements are not evidence for the current wlroots renderer or every driver. Diagnose the current process and preserve its logs before making hardware claims. `QProcess: Cannot create pipe (Too many open files)` indicates resource exhaustion, not necessarily a missing executable.

## Built-in GLSL

The standalone EGL render-element library embeds explicitly listed shaders from `src/compositor/renderer/opengl/shaders/` through a CMake-generated C++ header. `ShaderAssetLoader` recognizes vertex/fragment/geometry/compute/tessellation suffixes and aliases, including `.glsl`, `.glal` and `.shader` when a stage suffix or `#pragma ludash_stage` supplies the stage. The loader adds an appropriate GLSL version when absent; the active EGL context must support it.

Renderer tests check embedded resource relocation, software OpenGL and failure cleanup. They do not prove absence of all GPU/fence leaks or that those standalone GL passes are connected to the wlroots scene. See [graphics](GRAPHICS.md), [effects](EFFECTS.md) and [testing](TESTING_AND_FILES.md).


## Shared surface design

The 1.0.1a shell uses one desktop design language for Dashboard, Settings, wallpaper pickers and portal wrappers: strong outer surfaces, glass cards, hairline borders, shared radii and bounded layouts. Desktop-visible plugins should consume host theme/context values instead of inventing unrelated window chrome.

Dashboard telemetry uses bounded layouts rather than absolute text placement. Settings supports a maximized presentation without changing page contracts. Desktop widget plugins render in the wallpaper Background surface.
