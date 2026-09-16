# Screen capture

LunaDash renders every surface into one window, so a capture is a region of that window's content. Three parts of the stack are involved: the `zwlr_screencopy_manager_v1` global for clients, the `zxdg_output_manager_v1` and `zwp_idle_inhibit_manager_v1` globals that capture tools and video players probe first, and the session's own capture action for the keyboard shortcut and the control socket.

## Session capture

| Path | Behaviour |
| --- | --- |
| `Alt` + `Shift` + `F5` | Default binding of the `screenshot` shortcut action; rebindable and disableable in Settings |
| `lunadashctl screenshot` | Same action over the control socket; returns the written path |
| `lunadashctl capture /absolute/path.png` | Writes to an explicit path and refuses to overwrite an existing file |

A capture is one PNG with the compositor's logical size, written to `${XDG_PICTURES_DIR:-$HOME}/Screenshots/lunadash-<yyyyMMdd-HHmmss>.png`. A second capture inside the same second gets a `-1`, `-2` suffix instead of overwriting the first. The status field `screenCapture.lastCapture` names the newest file, and `screenCapture.error` explains a failure. Failed captures never remove an earlier file.

The shell does not yet host notifications, so a capture produces no on-screen confirmation; the session log records `LunaDash screenshot: <path>`.

## Client protocol coverage

`zwlr_screencopy_manager_v1` is announced at version 3. It serves the `wl_shm` path:

- `buffer` reports `WL_SHM_FORMAT_XRGB8888` with the logical width, height and `4 * width` stride. Version 3 clients also receive `buffer_done`; version 1 and 2 clients get the guaranteed shm `buffer` event alone.
- `copy` and `copy_with_damage` are both served. `copy_with_damage` reports the whole region as damage, which is the conservative answer for a compositor that copies the current content.
- `flags` reports zero: the copied rows are already in the buffer's order, so no `y_invert` is requested.
- `ready` carries a `CLOCK_MONOTONIC` timestamp; a frame whose region is empty or larger than the output is answered with `failed`.
- Invalid buffers (a non-`wl_shm` buffer, a mismatched size or a stride below `4 * width`) are rejected with the protocol's `invalid_buffer` error, and a repeated copy with `already_used`.

Known limits of this implementation:

- **No linux-dmabuf frames.** The version 3 `linux_dmabuf` event is never sent, so clients that require dmabuf capture do not work. `wl-screenrec` reports `your compositor does not support zwp-linux-dmabuf`.
- **No cursor overlay.** `overlay_cursor` is accepted but ignored, because LunaDash has no separate cursor layer to composite. The captured frame never includes the pointer.
- **Region capture only.** There is no per-surface or per-toplevel capture, and no `ext-image-copy-capture` support.

## Why common capture tools still fail

`grim`, `slurp` and `wf-recorder` bind `wl_output` above the version the compositor announces, and `libwayland` rejects the whole connection before the client reaches screencopy:

```
wl_registry#2: error 0: invalid version for global wl_output (7): expected at most 2, got 3
```

The versions Qt Wayland Compositor announces are fixed in Qt alone; `QWaylandOutput::initialize()` calls `d->init(d->compositor->display(), 2)` and exposes no API to raise the global, so LunaDash cannot advertise `wl_output` version 3 or 4 on top of Qt. The same version is still hardcoded on Qt's development branch. Several other clients tolerate the older global and reach screencopy normally, and the session capture path above works regardless.

`xdg-output` is announced together with an object for the output, because Qt fails a client request with a protocol error when a manager exists without one. `idle-inhibit` is announced as well; LunaDash has no idle blanking or screen locking yet, so every inhibition is trivially satisfied.

## Verification

`tests/wayland/test_screen_capture.py` runs a nested session, checks that the three globals are announced at their expected versions, asserts that `wl_output` is still at version 2, drives `Alt` + `Shift` + `F5` through the keyboard with `xdotool`, and verifies the written PNG, the explicit `capture` path, its overwrite refusal and the reported status. It needs `wayland-utils`, `xdotool` and Pillow.

References: [wlr-screencopy-unstable-v1](https://gitlab.freedesktop.org/wlroots/wlr-protocols/-/blob/master/unstable/wlr-screencopy-unstable-v1.xml), [xdg-output-unstable-v1](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/unstable/xdg-output/xdg-output-unstable-v1.xml).
