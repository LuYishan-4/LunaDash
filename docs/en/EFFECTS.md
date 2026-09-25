# Window motion and renderer effects

The active wlroots compositor uses `src/compositor/window/animation/SceneAnimationBackend` for window transitions. The default 220 ms transition fades and gently lifts mapped windows; closing/unmapping windows retain a scene snapshot so the animation can finish after the surface disappears. Moving windows updates their scene position. Closing does not bypass an application's save/cancel dialog. Disable animations or set duration to zero for reduced motion.

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl appearance '{"animations":true,"animationDuration":220}'
./build/lunadashctl appearance '{"animations":false}'
```

`effects.activeAnimations` reports live scene transitions. Listener teardown and retained snapshot ownership preserve the remote wlroots lifecycle fixes, including the distinct xdg role event boundary in wlroots 0.18. The xdg lifecycle integration test maps a real client before closing it and checks that the compositor remains alive.

## Application frosted glass

The existing wlroots scene now renders a blurred backdrop below ordinary Wayland and XWayland application windows. It applies by window role rather than by an application allowlist, so native, GTK, Qt, Electron and XWayland clients share the same path. Application text and controls are composited over the backdrop without being blurred. A fully opaque application reveals the backdrop only when **Window opacity** is below 100%; new profiles use 90%. Existing saved opacity remains authoritative.

Settings > Visual effects controls **Background blur**, **Blur strength** (0–32) and **Window opacity** (60–100%). Zero strength or disabling blur removes the backdrop. Fullscreen and eye-care mode remain opaque. A client that already provides transparency can use 100% window opacity while still revealing its blurred backdrop. Reducing global opacity also makes text less opaque; this setting does not identify or replace each application's background color.

`renderer/blur/SceneBackdrop.c` uses the active wlroots renderer to capture only the scene content below a window, including overlapping lower windows. It excludes the window itself and its previous backdrop, then performs horizontal and vertical nine-tap Gaussian passes. The capture is reduced in resolution and bounded to 1024 pixels per axis, with padding before the blur and cropping afterward. It does not read frames back to the CPU or replace the renderer. GLES uses the existing renderer; pixman can exercise the same composition path in CI. `WindowGlass` owns the per-window scene underlays, reuses unchanged captures and follows scene-node lifetimes. Native effect plugins remain disabled by default.

The status reports actual `blurReady`, `blurFailed`, `blurFrames` and the blur error. Unsupported buffer imports or render-target allocation failures keep the application usable and report failure. This is a development effect, not evidence that every hardware driver, HDR/color-management path, video overlay or application has been verified. Layer-shell panels and special fullscreen surfaces are outside the ordinary application-window effect.

On wlroots 0.19 and newer, a lower source window using explicit synchronization disables the backdrop that would sample it. The optional effect does not queue that extra read until asynchronous commit-release ownership is implemented. An explicit-sync target window can still show a backdrop composed from implicit-sync content below it, since the target itself is never sampled. Normal application rendering continues through wlroots.

The separate Qt render-element library still retains its OpenGL blur, procedural wallpaper and decoration shaders under `renderer/opengl`. It is not substituted for the active wlroots scene. CI includes pixel-level backdrop isolation, Gaussian smoothing, bounds and ownership regression tests; the Arch shell workflow captures the actual software Wayland session. Physical GPU performance and a real login-session screenshot remain required for final visual verification.

Quickshell independently animates shell surfaces and image wallpaper changes. Visual validation, application compatibility and GPU performance still require a real session. The development implementation does not claim full KWin effect compatibility. See [graphics](GRAPHICS.md) for shader/lifetime tests and [architecture](ARCHITECTURE.md) for ownership.

## NyxNiri-inspired desktop, Orbit and live wallpapers

See [the desktop integration guide](NYXNIRI_DESKTOP.md) for the new modules, palette and portal services, shortcuts, dependencies, and current verification limits.
