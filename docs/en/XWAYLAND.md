# X11 application compatibility

LunaDash remains a Wayland compositor. Optional XWayland is provided by wlroots in **lazy, rootless XWM mode**. The compositor creates one `wlr_xwayland`, binds it to the same `wlr_seat` used by native Wayland clients, and exports the reserved `DISPLAY`. Xwayland itself starts only when an X11 client connects.

On Arch:

```sh
sudo pacman -S --needed xorg-xwayland
```

Set `LUDASH_DISABLE_XWAYLAND=1` before compositor startup to disable X11 compatibility.

## Window integration

X11 windows are not placed inside a rootful compatibility container. wlroots' XWM exposes each `wlr_xwayland_surface` to LunaDash, and each associated X11 surface becomes a normal `ClientWindow`.

That means an X11 window participates independently in:

- workspaces and the taskbar;
- focus and Alt+Tab;
- tiling/freeform placement;
- minimize, maximize, fullscreen and close;
- compositor window rules and the normal scene graph.

The X11 surface lifecycle is handled as `new_surface → associate → wl_surface map/unmap → dissociate → destroy`. LunaDash never assumes the inner `wlr_surface` exists before `associate` or after `dissociate`.

## Clipboard and selection bridge

The XWM is bound to LunaDash's normal seat with `wlr_xwayland_set_seat()`. wlroots then bridges X11 CLIPBOARD/selection transfers to the Wayland seat. The same seat is also published through `wl_data_device_manager` and `zwlr_data_control_manager_v1`.

As a result, clipboard ownership is one compositor domain:

```text
X11 application
    ↕
wlroots XWM selection bridge
    ↕
wlr_seat
    ↕
Wayland data-device / data-control
    ↕
Wayland application + LunaDash clipboard history
```

The clipboard history watcher is MIME-aware. It records text and URI/file lists directly; image, audio, video and other binary payloads are stored in owner-only files under `$XDG_RUNTIME_DIR/lunadash/clipboard-payloads/`. Selecting a history item re-offers the original MIME type instead of converting everything to text.

`tests/wayland/test_xwayland.py` verifies both directions:

- X11 `xclip` → Wayland `wl-paste` and LunaDash history;
- Wayland `wl-copy` → X11 `xclip`.

## Launching X11 and hybrid applications

An explicit X11 process can be launched with:

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
lunadashctl launch-x11 'application --argument'
```

That child receives the reserved `DISPLAY`, X11 toolkit backends and no `WAYLAND_DISPLAY`. Native Wayland applications which also need an X11 helper use:

```sh
lunadashctl launch-with-x11 -- application --argument
```

The hybrid path keeps `WAYLAND_DISPLAY` and also exposes `DISPLAY`. The launch-capability registry in `data/session/launch-capabilities.json` decides which application identities request this capability; compositor code does not rewrite Chromium/Electron renderer flags by executable name.

Because XWayland is lazy, merely inheriting `DISPLAY` does not start the X server. The first real X11 connection starts Xwayland and the XWM.

## Session environment and lifecycle

After creating XWayland, LunaDash publishes `DISPLAY` together with the Wayland/session variables to the D-Bus activation environment and systemd user manager. Applications started indirectly through portals, D-Bus activation or user units therefore see the same XWayland display.

Status exposes:

- `xwayland.available`
- `xwayland.running`
- `xwayland.mode = "rootless-lazy-xwm"`
- `xwayland.rootWindowVisible = false`
- `xwayland.selectionBridge`
- `xwayland.display`
- `xwayland.error`

There is no LunaDash-managed Xauthority file or rootful X11 desktop window in this mode. wlroots owns the Xwayland server/XWM lifecycle. During logout or tests, LunaDash closes both XDG and X11 client windows before destroying the XWayland bridge.

## Testing

The integration test is:

```sh
xvfb-run -a python3 tests/wayland/test_xwayland.py build
```

It checks lazy startup, a first-class X11 window, bidirectional clipboard bridging, clipboard-history capture, and clean shutdown. The main Wayland CI also checks that `zwlr_data_control_manager_v1` is advertised.

X11 remains an optional compatibility path. Application-specific input grabs, unusual DnD formats, Wine/Java edge cases and multi-output behavior still require real-session testing.
