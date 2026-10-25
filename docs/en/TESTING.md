# Testing LunaDash

Use the maintained [testing and source map](TESTING_AND_FILES.md) for build, static, runtime and staged-install commands. [Graphics](GRAPHICS.md) distinguishes the Qt OpenGL render-element tests from the active wlroots pixman session checks.

## Run the CI suites locally

```sh
./scripts/run-tests.sh              # contracts, CTest and the headless runtime sessions
./scripts/run-tests.sh contracts    # no build: repository, source and packaging checks
./scripts/run-tests.sh all          # also the installer suites
```

The runner builds into `build-tests/` (never `build/`), fetches the pinned LunaDash-Shell and
SlintLu there unless `LUDASH_SHELL_SOURCE_DIR` and `LUDASH_SLINTLU_SOURCE_DIR` name checkouts,
keeps every check's output in `build-tests/test-logs/` and ends with a pass/fail summary.
Runtime sessions are headless, run on a private D-Bus without `WAYLAND_DISPLAY`, `DISPLAY` or
`XDG_SESSION_ID`, so they cannot show notifications, take over portals or register a polkit
agent on the desktop you start them from. `--no-build` reuses an existing build and
`--build-dir DIR` picks another one.

For an existing Wayland desktop, `./scripts/test-once.sh` builds and starts a nested session. Its output is `build-once/wayland.log` and `build-once/wayland-state.json`; this helper does not currently capture a screenshot. Inspect a real session for Chrome/Zed, physical input, wallpaper/animation and portal behavior before release promotion.


The `Arch Quickshell desktop layout` workflow starts the actual shell on a headless Wayland compositor and uploads desktop/control-center PNGs at 1920×1080 and 1280×720, protocol logs and JSON status. It checks popup bounds, QML runtime errors and the absent default dock. These are software-session artifacts; they do not replace a physical GPU/login screenshot. For this change, local builds and tests were explicitly prohibited; validation runs in CI only.
The same job opens six Dolphin Qt Wayland windows, checks usable-area bounds, non-overlap, preferred minimum sizes and actual client geometry, toggles blur, and compares corner pixels against the desktop. It exercises the real Settings controls through Qt accessibility actions and Wayland keyboard input, including panel dimensions, content switches and custom launcher image/restore. Evidence includes the resulting Settings and desktop screenshots, plus a dark/light capsule comparison. Corner coverage and glass ownership also have native CI regression tests.

Workspace regression coverage checks the two initial destinations, opening a Dolphin window on workspace 2 and retaining workspace 3 after returning to workspace 1. Captured pill-center pixels verify alignment; QML tests also cover minimized clients, empty-spare visits, workspace gaps and configured limits.
