# X11 application compatibility

LunaDash remains a Wayland compositor. Optional XWayland runs a rootful compatibility window that uses LunaDash's normal tiling, focus and effects. X11 applications share the inside of this window; they are not individually tiled. LunaDash does not implement a separate X11 window manager.

On Arch:

```sh
sudo pacman -S --needed xorg-xwayland
```

Restart LunaDash after installing the optional packages. The compositor registers `wp_viewporter`, prepares a private display and starts XWayland on the first explicit X11 launch. Native Wayland applications continue to use Wayland. Missing compatibility tools do not prevent the native desktop from starting.

The launcher and settings offer **Run an X11 application**. Enter a program and arguments, for example `xterm` if installed. Quotes group arguments; shell operators are not evaluated. The same action is available through IPC:

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl launch-x11 'application --argument'
./build/lunadashctl status
```

This launch path sets `DISPLAY`, `XAUTHORITY`, `QT_QPA_PLATFORM=xcb`, `GDK_BACKEND=x11` and the SDL X11 backend. It removes `WAYLAND_DISPLAY` for that child. Java clients receive `_JAVA_AWT_WM_NONREPARENTING=1`.

## Isolation and lifecycle

LunaDash reserves an unused local X socket, restricts it to its owner and passes the listening descriptor to XWayland. It creates an owner-only MIT-MAGIC-COOKIE-1 authority file using system randomness and disables TCP listening. It does not use `-ac`, change the host display or overwrite the host authority file. Authentication is not an application sandbox: X11 applications sharing this instance retain the usual X11 trust model.

Only this compatibility instance's process group is terminated during cleanup. The runtime authority file and owned socket are removed on normal shutdown. Status exposes `xwayland.available`, `xwayland.running`, `xwayland.mode`, its display/authority path and a diagnostic error. No cookie value is sent over IPC or logged. Set `LUDASH_DISABLE_XWAYLAND=1` before startup to disable the optional service.

The integration test launches a Qt application through its XCB backend, verifies it becomes a Wayland window, rejects an unauthenticated X11 setup request, and checks cleanup:

```sh
xvfb-run -a python3 tests/wayland/test_xwayland.py build
```

## Limits

The current Qt xdg-shell global negotiates version 1. Satellite 0.8.2 requires version 2 or newer, so rootless satellite integration is not enabled. The compatibility container has no internal X11 window manager: internal stacking, positioning and decorations remain limited. Save work and close X11 applications before closing their shared container: closing the container ends its X server and disconnects all applications inside it.

Compatibility depends on the installed XWayland version and the application's protocol use. Clipboard, drag-and-drop, games, input grabs, HiDPI, menus and Wine/Java applications need broader application-specific testing. The integration test is not evidence that every legacy application works. Multiple outputs, pointer constraints and full portal integration remain incomplete in LunaDash.

References: [xwayland-satellite](https://github.com/Supreeeme/xwayland-satellite), [Wayland X11 compatibility architecture](https://wayland.freedesktop.org/docs/book/Xwayland.html).
