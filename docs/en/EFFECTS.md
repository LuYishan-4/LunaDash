# Window motion and renderer effects

The active wlroots compositor uses `src/compositor/animation/SceneWindowAnimations` for window transitions. The default 220 ms transition fades and gently lifts mapped windows; closing/unmapping windows retain a scene snapshot so the animation can finish after the surface disappears. Moving windows updates their scene position. Closing does not bypass an application's save/cancel dialog. Disable animations or set duration to zero for reduced motion.

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl appearance '{"animations":true,"animationDuration":220}'
./build/lunadashctl appearance '{"animations":false}'
```

`effects.activeAnimations` reports live scene transitions. Listener teardown and retained snapshot ownership preserve the remote wlroots lifecycle fixes, including the distinct xdg role event boundary in wlroots 0.18. The xdg lifecycle integration test maps a real client before closing it and checks that the compositor remains alive.

The Qt render-element library retains two-pass Gaussian blur, procedural wallpaper and decoration shaders in `src/compositor/renderer/opengl`. Its scene-graph adapter clips transformed blur geometry and performs GL resource work while Qt's render-thread context is current. This library is built and tested separately; it is not currently wired into the active wlroots scene. Consequently blur preferences alone do not demonstrate rendered background blur, and the wlroots status currently reports `blurReady: false`.

Quickshell independently animates shell surfaces and image wallpaper changes. Visual validation, application compatibility and GPU performance still require a real session. The development implementation does not claim full KWin effect compatibility. See [graphics](GRAPHICS.md) for shader/lifetime tests and [architecture](ARCHITECTURE.md) for ownership.

## NyxNiri-inspired desktop, Orbit and live wallpapers

See [the desktop integration guide](NYXNIRI_DESKTOP.md) for the new modules, palette and portal services, shortcuts, dependencies, and current verification limits.
