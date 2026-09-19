# Display controls and startup

## Brightness

Settings → Display reads the machine-readable `brightnessctl` percentage field and selects the `backlight` class explicitly, avoiding keyboard LEDs and the maximum-brightness field. Changes target the detected device, run asynchronously and coalesce repeated slider movements. The installer includes brightnessctl. A rejected write keeps the control available and reports the permission error; LunaDash does not run the desktop as root or change device permissions.

`lunadashctl brightness 60` requests 60% brightness. Status exposes `brightness.available`, `device`, `percent`, `busy` and `error`. External monitors without a kernel backlight still require DDC/CI controls; this change does not implement a DDC/CI service.

## Resolution, refresh rate and scale

Settings → Display lists modes advertised by the primary wlroots output and supports scale factors from 100% to 300%. The compositor tests and commits the configuration. Choose **Keep changes** within 15 seconds to save it; otherwise the previous mode and scale are restored. **Revert** restores them immediately. Confirmed physical-output settings are restored on the next startup, with fallback to a supported preferred mode if the driver refuses them.

On an unconfigured physical display, LunaDash chooses the highest advertised refresh rate at the preferred resolution. Nested windows use the host's pacing instead of requesting a fixed 60 Hz custom mode. Nested physical resolution/refresh rate still belong to the host desktop; LunaDash exposes its window size and client scale. This is primary-output configuration, not a complete multi-monitor arrangement interface.

```sh
lunadashctl display-configure '{"scale":1.25}'
lunadashctl display-confirm
# Use an exact mode ID from status.display.modes:
lunadashctl display-configure '{"mode":"1920x1080@144000"}'
lunadashctl display-revert
```

Unavailable modes and invalid scales are rejected. Output removal cancels a pending confirmation. Status reports the physical pixel size, logical size, refresh rate, available modes and remaining confirmation time.

## Startup and animation

The compositor uses a neutral dark fallback while the shell maps, replacing the old blue background. D-Bus/systemd activation environment publication is asynchronous, so a slow service no longer blocks the Wayland event loop. The shell receives the initial wallpaper path directly, avoiding a first-status delay.

The shell shows the LunaDash logo and a short loading animation while waiting for compositor state and wallpaper readiness, then fades into the desktop. Slow startup exposes a message and a dismiss action. Reduced-motion settings disable the pulse/fade. This is the desktop session startup animation, not a firmware or Plymouth boot splash.

Hidden panels load lazily. Buffer-only shell commits no longer rearrange every client or repeatedly steal keyboard focus from the active region selector. Wallpaper decode size follows the output size/scale, and its transition mask is disabled when idle. Native window effects advance from output frame callbacks instead of a separate Qt animation timer; active effects schedule another frame, and idle scenes do not repaint continuously.

Use `lunadash-compositor --profile` to report event-loop stalls. Automated tests cover asynchronous startup, control geometry with translated labels, and headless display rollback. Actual FPS, physical refresh rate switching and backlight writes require testing on the target hardware; CI alone does not establish those results.
