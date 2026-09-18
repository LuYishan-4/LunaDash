# Background blur and motion

New application windows use background blur by default. The compositor captures the already-rendered area behind each window, downsamples it, applies horizontal and vertical Gaussian passes at half resolution, interpolates the result back to the window size, then paints the client's content. This blurs the backdrop rather than the application's text. The implementation supports desktop OpenGL and OpenGL ES through the C rendering core.

The default window opacity is 96%, so blur is visible behind otherwise opaque clients too. Set opacity to 100% to preserve an application's opaque pixels; its transparent regions can still reveal blur. Blur strength is 18 by default and can be set from 0 to 32 or disabled. The current effect is rectangular and does not implement KDE's per-region blur protocol. Shell panels use translucent rounded surfaces; the C blur pass currently applies to application frames.

Open Desktop settings → Glass and motion to change blur, opacity, animations and animation duration. Values are saved immediately. The default 220 ms compositor transitions fade and gently lift windows when they open. Closing/unmapping windows fade out from a retained wlroots scene snapshot so the animation can finish after the client surface disappears. Shell panels animate their reveal, and buttons respond to hover/press. Animations do not delay a close request or bypass an application's save/cancel dialog. Turn them off, or set duration to zero, for reduced motion.

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl appearance '{"blur":true,"blurRadius":18,"windowOpacity":96}'
./build/lunadashctl appearance '{"animations":true,"animationDuration":220}'
./build/lunadashctl appearance '{"blur":false,"windowOpacity":100,"animations":false}'
```

`blurReady`, `blurFailed`, `blurFrames` and `activeAnimations` in status JSON provide test evidence. A disabled effect is not a renderer failure. Image inspection still matters: successful shader compilation alone does not prove the backdrop is visually correct.

Resources belong to the current Qt render-thread context. The C++ adapters release C rendering objects during scene-graph teardown. Animation ownership follows the affected item; canceled transitions and destroyed items must not leave callbacks referencing dead objects. The controller disconnects canceled completions before stopping a group. Delayed deletion of an old group cannot remove a replacement transition, and destroying the controller first cancels its remaining groups. Tests cover these lifetimes, completion callbacks that destroy items, and the C shaders under GL/GLES.

Blur capture skips empty or non-finite transformed geometry and clips finite coordinates to the framebuffer before integer conversion. This avoids undefined float-to-integer conversions during transient scene-graph states. The `blur-geometry` regression covers infinity, NaN, huge coordinates, pixel scaling and partial clipping; session tests still require successful blur rendering after startup.

The render node copies its transformed rectangle in `prepare()`. Qt 6.4's [RHI batch renderer](https://github.com/qt/qtdeclarative/blob/v6.4.2/src/quick/scenegraph/coreapi/qsgbatchrenderer.cpp) exposes a stack-backed model-view matrix during preparation and calls `render()` later. Reading that pointer in `render()` can produce invalid geometry. LunaDash retains the copied rectangle instead; both older and current Qt session tests must report rendered blur frames.

Physical GPU performance and complex clipping/scaling still need broader testing. Lower blur strength or disable blur when GPU cost is a concern. This is a development implementation, not a claim of full KWin effect compatibility.

Gaussian weights are computed once per draw in the C core, normalized, and uploaded as uniforms. Adjacent taps are paired using linear filtering, preserving the dense kernel while reducing texture reads. Both convolution passes run at half resolution; the full-resolution composition uses a single filtered sample and still honors opacity, scissor and stencil state. This avoids per-pixel exponential calculations and the previous full-resolution vertical convolution, which stalled Mesa rendering during two-CPU container interaction tests. GL/GLES image tests check smooth falloff in both axes, symmetry and preservation of a constant image.
