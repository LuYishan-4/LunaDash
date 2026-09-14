# Shell rendering and NVIDIA compatibility

The compositor and Quickshell render in separate processes. `--graphics opengl|gles` selects the compositor API and its wallpaper/blur shaders. `LUDASH_SHELL_RENDERER` selects only Quickshell's Qt Quick backend:

| Value | Behavior |
| --- | --- |
| `auto` (default) | Software when `/proc/driver/nvidia/version` exists; OpenGL otherwise. |
| `software` | CPU-rendered Quickshell surfaces; the compositor retains its requested GPU API and effects. |
| `opengl` | GPU-rendered Quickshell, including on NVIDIA. |

Detection is deliberately conservative on hybrid systems with a loaded NVIDIA driver. Explicit overrides take precedence. Invalid values fail startup with code 2. The selected backend is logged as `LuDash shell renderer: ...`. Changes apply when starting a new session.

```sh
# Nested session from an existing Wayland desktop:
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE \
  LUDASH_SHELL_RENDERER=software QT_QPA_PLATFORM=wayland \
  ./build/lunadash-compositor --graphics gles

# Manual GPU-shell comparison on a driver that supports it:
LUDASH_SHELL_RENDERER=opengl LUDASH_TEST_HOST_WAYLAND=1 \
  python3 tests/wayland/test_resource_lifetime.py build
```

Software rendering can increase CPU usage. Custom QML modules using GPU-only effects must provide their own software fallback. The bundled LunaDash logo therefore reveals with opacity only in this mode: an animated scale or rotation leaves stale pixels behind because the software renderer's damage tracking does not cover the old transformed bounds. This compatibility mode limits the affected shell buffer path; it does not repair the driver or guarantee resource safety for other GPU-rendered Wayland applications.

## Why the default changed

On the tested NVIDIA 615.71.09 / Qt 6.11.2 host, GPU-backed Quickshell and the compositor accumulated `anon_inode:sync_file` descriptors during sustained rendering. Eventually, helpers could no longer create pipes. Quickshell's generic `likely because the binary could not be found` message was accompanied by `QProcess: Cannot create pipe (Too many open files)`; reinstalling `lunadashctl` would not fix descriptor exhaustion.

A 40-second diagnostic comparison with software Quickshell kept its fence count at zero and the compositor at two or three fences before teardown. Disabling explicit synchronization alone did not prevent growth in the longer comparison, so LunaDash does not set that driver workaround. These observations establish an effective workaround on the tested configuration, not the exact defect inside the driver or Qt. Raising the descriptor limit only delays exhaustion. Rebuild and restart LunaDash to apply the fix to an existing session.

## Sustained resource regression

```sh
# Headless Mesa check, also configured in Arch CI:
xvfb-run -a -s '-screen 0 1440x900x24' \
  python3 tests/wayland/test_resource_lifetime.py build

# Actual host Wayland/GPU path, with automatic shell compatibility selection:
LUDASH_TEST_HOST_WAYLAND=1 python3 tests/wayland/test_resource_lifetime.py build
```

The test uses private runtime/configuration directories, creates three native clients, changes the accent every second and samples only its own compositor and Quickshell. After a six-second warmup, growth above 96 file descriptors or 32 synchronization fences fails the test. It also requires a normal session exit and rejects graphics/pipe-exhaustion diagnostics. `LUDASH_SOAK_SECONDS` accepts 30 through 180 seconds; the default is 45 seconds plus startup and shutdown time.

Evidence is saved as `build/ci-evidence/resource-host.{log,json}` or `resource-software.{log,json}`. This catches the observed sustained leak that an eight-second smoke test missed. Bounded counts over one short run do not prove absence of every GPU or memory leak; ASan leak detection remains disabled separately.
