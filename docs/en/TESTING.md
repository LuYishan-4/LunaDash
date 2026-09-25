# Testing LunaDash

Use the maintained [testing and source map](TESTING_AND_FILES.md) for build, static, runtime and staged-install commands. [Graphics](GRAPHICS.md) distinguishes the Qt OpenGL render-element tests from the active wlroots pixman session checks.

For an existing Wayland desktop, `./scripts/test-once.sh` builds and starts a nested session. Its output is `build-once/wayland.log` and `build-once/wayland-state.json`; this helper does not currently capture a screenshot. Inspect a real session for Chrome/Zed, physical input, wallpaper/animation and portal behavior before release promotion.


The `Arch Quickshell desktop layout` workflow starts the actual shell on a headless Wayland compositor and uploads desktop/control-center PNGs at 1920×1080 and 1280×720, protocol logs and JSON status. It checks popup bounds, QML runtime errors and the absent default dock. These are software-session artifacts; they do not replace a physical GPU/login screenshot. For this change, local builds and tests were explicitly prohibited; validation runs in CI only.
 The same job also opens six Qt Wayland file-manager windows, checks that recursive splits preserve the right-hand main pane, toggles real blur and uploads frosted/opaque screenshots.
