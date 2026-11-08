# Portal runtime and source selection

LunaDash keeps its own file chooser and screen-source selection UI. The file
picker's wrapper, manual path entry, filters and shared desktop theme are not
replaced by Dolphin or KDE. ScreenCast still uses xdg-desktop-portal-wlr and
PipeWire; the LunaDash source picker is not itself a video streaming backend.

## Login routing

The session launcher reuses DBUS_SESSION_BUS_ADDRESS when present. If it is
absent but the PAM user bus exists at $XDG_RUNTIME_DIR/bus, the launcher uses
that bus rather than creating a private bus disconnected from systemd services.

On an installed login, when no explicit user LunaDash configuration exists,
the launcher copies the installed defaults to
$XDG_CONFIG_HOME/xdg-desktop-portal/lunadash-portals.conf (normally under
~/.config). Desktop-specific configuration prevents a generic portals.conf
left by another desktop from selecting its backend in LunaDash. The generic
file and existing LunaDash-specific files, including symlinks, are preserved.
The --check preflight never creates configuration files.

FileChooser and Settings route to lunadash; ScreenCast and Screenshot route to
wlr. An existing explicit LunaDash override can still select a different backend.
Inspect that file when an upgrade does not change routing.

## Backend contracts

The D-Bus backend uses GLib GDBus and owns its service name only after it has
exported FileChooser version 4 and Settings version 2. FileChooser and
ScreenCast each launch a separate Slint/Winit Wayland client. The FileChooser
helper receives JSON on stdin and returns the accepted local paths and portal
options on stdout. `Request.Close` terminates the active picker process.
Readiness and activation failures are written to stderr/journal.

The dmenu chooser must return the selected input line unchanged. In particular,
UTF-8 names and trailing spaces belong to the opaque source label. Trimming a
label can make xdpw reject a legitimate confirmation as an unknown selection.
Cancellation returns no label. Thumbnail capture failure must not block selection.

## Verification boundaries

No workflow runs the portal backend; CI only builds it. Physical-session focus,
Flatpak application identification and sustained NVIDIA/PipeWire streaming must be
checked in a physical session. Capture protocol and source-type support does not
prove that a complete Discord call works.

## Physical-session diagnostics

Inside the affected LunaDash session, capture the portal journals as well as the
compositor log:

```sh
journalctl --user -b --no-pager -n 250 \
  -u xdg-desktop-portal.service \
  -u xdg-desktop-portal-lunadash.service \
  -u xdg-desktop-portal-wlr.service
```

A client-side "Request ended (non-user cancelled)" does not identify which
backend failed. An "Unable to open /proc/<pid>/root" message is an application
identification/permission failure; it is not proof of a renderer failure. Do not
disable sandboxing, ptrace protections, or grant broad Flatpak host access to hide
it. Confirm the requested PID, effective backend and user bus in the journals.
