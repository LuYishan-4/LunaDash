# Background blur and motion

New application windows use background blur by default. The compositor captures the already-rendered area behind each window, downsamples it, applies horizontal and vertical densely sampled Gaussian passes, then paints the client's content. This blurs the backdrop rather than the application's text. The implementation supports desktop OpenGL and OpenGL ES through the C rendering core.

The default window opacity is 96%, so blur is visible behind otherwise opaque clients too. Set opacity to 100% to preserve an application's opaque pixels; its transparent regions can still reveal blur. Blur strength is 18 by default and can be set from 0 to 32 or disabled. The current effect is rectangular and does not implement KDE's per-region blur protocol. Shell panels use translucent rounded surfaces; the C blur pass currently applies to application frames.

Open Desktop settings → Glass and motion to change blur, opacity, animations and animation duration. Values are saved immediately. The default 220 ms transitions fade/scale windows when opened, closed, minimized, restored or shown/hidden by workspace switches. Shell panels animate their reveal, and buttons respond to hover/press. Animations do not delay a close request or bypass an application's save/cancel dialog. Turn them off, or set duration to zero, for reduced motion.

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/ludashctl appearance '{"blur":true,"blurRadius":18,"windowOpacity":96}'
./build/ludashctl appearance '{"animations":true,"animationDuration":220}'
./build/ludashctl appearance '{"blur":false,"windowOpacity":100,"animations":false}'
```

`blurReady`, `blurFailed`, `blurFrames` and `activeAnimations` in status JSON provide test evidence. A disabled effect is not a renderer failure. Image inspection still matters: successful shader compilation alone does not prove the backdrop is visually correct.

Resources belong to the current Qt render-thread context. The C++ adapters release C rendering objects during scene-graph teardown. Animation ownership follows the affected item; canceled transitions and destroyed items must not leave callbacks referencing dead objects. Tests cover those cases and the C shaders under GL/GLES.

Physical GPU performance and complex clipping/scaling still need broader testing. Lower blur strength or disable blur when GPU cost is a concern. This is a development implementation, not a claim of full KWin effect compatibility.
