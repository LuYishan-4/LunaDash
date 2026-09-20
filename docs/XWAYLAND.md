# X11 application compatibility

LunaDash remains a Wayland compositor. Optional XWayland runs a rootful compatibility window that uses LunaDash's normal tiling, focus and effects. X11 applications share the inside of this window; they are not individually tiled. LunaDash does not implement a separate X11 window manager.

On Arch:

```sh
sudo pacman -S --needed xorg-xwayland
```

Restart LunaDash after installing the optional packages. The compositor registers `wp_viewporter` and prepares a private display. It starts XWayland for the first explicit X11 launch or for Discord's native input helper. Native Wayland applications continue to use Wayland. The client environment asks for `QT_QPA_PLATFORM=wayland;xcb`, `GDK_BACKEND=wayland,x11` and `SDL_VIDEODRIVER=wayland,x11`; `DISPLAY` is omitted until the authenticated compatibility server is running.

Discord's helper requires a usable X display even when Electron uses Wayland. LunaDash starts the service before launching Discord, supplies its authentication, and keeps the owned root surface out of tiling, focus and the taskbar until an explicit X11 application is requested. The compositor identifies that surface by the owned server process, not a client-supplied title. Later launches reuse the same server, display and authority file. A direct terminal launch bypasses this preparation; use the application launcher or the command in [Discord troubleshooting](LOGIN_SESSION.md#discord-and-external-application-menus).

Recognized Chromium browsers and Discord receive explicit native Wayland flags. Abnormal process exits are logged with the program name and exit status. Missing compatibility tools do not prevent the native desktop from starting; an application that requires the helper receives a launch error.

The launcher and settings offer **Run an X11 application**. Enter a program and arguments, for example `xterm` if installed. Quotes group arguments; shell operators are not evaluated. The same action is available through IPC:

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl launch-x11 'application --argument'
./build/lunadashctl status
```

This launch path sets `DISPLAY`, `XAUTHORITY`, `QT_QPA_PLATFORM=xcb`, `GDK_BACKEND=x11` and the SDL X11 backend. It removes `WAYLAND_DISPLAY` for that child. Java clients receive `_JAVA_AWT_WM_NONREPARENTING=1`. Every X11 launch also writes its exit status to the session log when the application ends abnormally.

Native Wayland applications that also need X11 helpers can request both connections explicitly:

```sh
lunadashctl launch-with-x11 -- application --argument
```

This generic launch path prepares the authenticated helper, preserves the native Wayland environment, and leaves a new helper's root window hidden. It reuses an existing server and does not hide a compatibility window that the user already opened. Direct IPC clients send method `launch-with-x11` with a JSON argument array encoded in `value`, just like `launch-command`. The CLI's `--` form preserves individual arguments without shell evaluation. Missing or disabled XWayland returns an error instead of starting the dependent application without its helper.

## Isolation and lifecycle

LunaDash reserves an unused local X socket, restricts it to its owner and passes the listening descriptor to XWayland. It creates an owner-only MIT-MAGIC-COOKIE-1 authority file using system randomness and disables TCP listening. It does not use `-ac`, change the host display or overwrite the host authority file. Authentication is not an application sandbox: X11 applications sharing this instance retain the usual X11 trust model.

Only this compatibility instance's process group is terminated during cleanup. The runtime authority file and owned socket are removed on normal shutdown. Status exposes `xwayland.available`, `xwayland.running`, `xwayland.mode`, `xwayland.rootWindowVisible`, its display/authority path and a diagnostic error. No cookie value is sent over IPC or logged. Set `LUDASH_DISABLE_XWAYLAND=1` before startup to disable the optional service.

The generic integration test launches Qt applications through XCB, verifies on-demand startup and display reuse, rejects an unauthenticated X11 setup request, and checks cleanup:

```sh
xvfb-run -a python3 tests/wayland/test_xwayland.py build
```

## Limits

Rootless satellite integration is not enabled. The compatibility container has no internal X11 window manager: internal stacking, positioning and decorations remain limited. Save work and close X11 applications before closing their shared container: closing the container ends its X server and disconnects all applications inside it.

Compatibility depends on the installed XWayland version and the application's protocol use. Clipboard, drag-and-drop, games, input grabs, HiDPI, menus and Wine/Java applications need broader application-specific testing. The integration test is not evidence that every legacy application works. Multiple outputs, pointer constraints and full portal integration remain incomplete in LunaDash.

References: [xwayland-satellite](https://github.com/Supreeeme/xwayland-satellite), [Wayland X11 compatibility architecture](https://wayland.freedesktop.org/docs/book/Xwayland.html).

## GPU Screen Recorder UI

The GTK frontend can show a message saying the new UI needs X11 when `XOpenDisplay` cannot connect, even inside a Wayland session. The Flatpak package also uses a host-side Wayland bridge for GPU and capture discovery. Flatpak exposes the session socket inside its sandbox as `wayland-0`; passing that name back to the host fails when the actual session uses a different name, such as `lunadash-0`.

For a native overlay above the desktop, update LunaDash and log into the updated session, then completely quit any existing recorder UI. Run this from a LunaDash terminal:

```sh
lunadashctl launch-with-x11 -- flatpak run --socket=x11 --command=env \
  com.dec05eba.gpu_screen_recorder \
  "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:?Run this from a LunaDash terminal}" \
  XDG_CURRENT_DESKTOP=river gsr-ui launch-show
```

GPU Screen Recorder UI's current native overlay selector recognizes a short list of desktop names rather than probing layer-shell support. `XDG_CURRENT_DESKTOP=river` is a compatibility override for this application process only: it selects the layer-shell overlay path already supported by LunaDash, without changing the desktop's identity. The UI still needs its X11 input and clipboard helpers even while drawing through Wayland. A direct native launch without those helpers can crash. Existing UI instances must exit first because a second launch forwards to the old instance instead of changing its backend.

`--command=env` restores the host socket name after Flatpak prepares the sandbox; Flatpak's `--env=WAYLAND_DISPLAY=...` alone is overwritten by its socket mapping. `launch-show` requests a visible UI instead of the default background launch. The `lunadashctl` JSON response reports desktop state, not the recorder's log or whether its window finished opening. Application output goes to the current `~/.local/state/lunadash/session-*.log`.

To check GPU and capture discovery without starting a recording:

```sh
flatpak run --command=env com.dec05eba.gpu_screen_recorder \
  "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:?Run this from a LunaDash terminal}" \
  gpu-screen-recorder --info
```

With Flatpak GPU Screen Recorder 6.1.2, the corrected discovery command returned successfully and listed the connected HDMI output. Native UI rendering was also verified: the application created a layer-shell OVERLAY surface at the full 1920×1080 output size and appeared above the taskbar and native application windows. The earlier `launch-x11` workaround contained its UI inside the shared XWayland window. Recording has not been verified. LunaDash's portal backend implements FileChooser, not PipeWire ScreenCast; the recorder's separate KMS monitor capture path is not the same as portal capture or capture of the XWayland compatibility window. See the [upstream UI notes](https://git.dec05eba.com/gpu-screen-recorder-ui/about/), [native backend selector](https://git.dec05eba.com/gpu-screen-recorder-ui/tree/src/Utils.cpp), and [host bridge implementation](https://git.dec05eba.com/gpu-screen-recorder-ui/tree/src/WaylandHostBridge.cpp) for platform limitations and connection handling.
