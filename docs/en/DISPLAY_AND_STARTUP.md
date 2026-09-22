# Display controls and startup

## Brightness

Settings → Display reads the machine-readable `brightnessctl` percentage field and selects the `backlight` class explicitly, avoiding keyboard LEDs and the maximum-brightness field. Changes target the detected device, run asynchronously and coalesce repeated slider movements. The installer includes brightnessctl. A rejected write keeps the control available and reports the permission error; LunaDash does not run the desktop as root or change device permissions.

`lunadashctl brightness 60` requests 60% brightness. Status exposes `brightness.available`, `device`, `percent`, `busy` and `error`. External monitors have separate DDC/CI controls below the internal-panel slider.

### External monitors (DDC/CI)

Install `ddcutil` (included by the dependency installer and Arch package), then enable DDC/CI in each monitor's on-screen menu. Settings → Display → External monitor brightness detects I2C displays, labels them individually and reads VCP brightness code `0x10`. Each slider targets its own bus; percentages are converted using that monitor's reported maximum, which need not be 100. Unsupported brightness features remain disabled with an explanation. Brightness reads and writes are matched to their monitor by the detection ID, so a background refresh cannot apply a result to the wrong display. Laptop backlight control continues to use brightnessctl independently.

Detection, reads and verified writes run asynchronously with bounded timeouts. Requests are serialized and repeated slider changes are coalesced per monitor. Detection refreshes every minute; **Refresh monitors** discovers connections or permission changes immediately. Failed writes retain the last confirmed value and report the error. USB HID monitor control is not included.

```sh
lunadashctl ddc-refresh
lunadashctl status
# Copy the exact ID from status.ddcBrightness.devices:
lunadashctl ddc-brightness '{"id":"i2c-7:DEL:DELL P2411H:F8NDP11G119U","percent":60}'
```

If detection fails, run `ddcutil detect --brief` as the session user. Check that the kernel `i2c-dev` module is loaded and the distribution's ddcutil udev rules grant access to the monitor's `/dev/i2c-*` device. After installing rules, reconnect the monitor or log out/in as required by the distribution. LunaDash does not run sudo, change device permissions or load kernel modules from the desktop. Some docks, drivers and monitor picture modes prevent DDC/CI access; check the monitor configuration and upstream troubleshooting.

Protocol output and setup references: [ddcutil detection](https://www.ddcutil.com/command_detect/), [brightness reads](https://www.ddcutil.com/command_getvcp/), [verified writes](https://www.ddcutil.com/command_setvcp/), [I2C permissions](https://www.ddcutil.com/i2c_permissions/). Verify detection, brightness reads/writes and reconnection on the target monitor; generic build checks do not establish physical monitor compatibility.

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

The shell uses a local Qt theme so portal startup cannot block its first frame. The FileChooser backend also avoids querying its own frontend during activation; the session selects the GTK backend for other supported portal interfaces instead of activating unrelated KDE/GNOME services. Install `xdg-desktop-portal-gtk` alongside `xdg-desktop-portal`.

The shell shows the LunaDash logo and a short loading animation while waiting for compositor state and wallpaper readiness, then fades into the desktop after at least 650 ms measured from its first rendered frame. Slow startup exposes a message and a dismiss action. Reduced-motion settings disable the pulse/fade. This is the desktop session startup animation, not a firmware or Plymouth boot splash.

Hidden panels load lazily. Buffer-only shell commits no longer rearrange every client or repeatedly steal keyboard focus from the active region selector. Wallpaper decode size follows the output size/scale, and its transition mask is disabled when idle. Native window effects advance from output frame callbacks instead of a separate Qt animation timer; active effects schedule another frame, and idle scenes do not repaint continuously.

Installed sessions load QML beside their installed executable before considering a source checkout. `lunadash-compositor --version` and the session log identify the compiled revision; source archives preserve that revision.

Use `lunadash-compositor --profile` to report event-loop stalls. The maintained CI covers general startup, protocol and rendering behavior. Actual FPS, physical refresh rate switching and backlight writes require testing on the target hardware; CI alone does not establish those results.
