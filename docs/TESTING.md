# Testing LunaDash

Use the maintained [testing and source map](TESTING_AND_FILES.md) for build, static, runtime and staged-install commands. [Graphics](GRAPHICS.md) distinguishes the Qt OpenGL render-element tests from the active wlroots pixman session checks.

For an existing Wayland desktop, `./scripts/test-once.sh` builds and starts a nested session. Its output is `build-once/wayland.log` and `build-once/wayland-state.json`; this helper does not currently capture a screenshot. Inspect a real session for Chrome/Zed, physical input, wallpaper/animation and portal behavior before release promotion.
