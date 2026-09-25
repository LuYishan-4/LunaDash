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

Both executable modes disable portal platform-theme/native-dialog recursion
before constructing QApplication. The backend registers D-Bus argument types,
exports FileChooser version 4 and the Settings adaptor, and only then acquires
its bus name. Readiness and activation failures are written to stderr/journal.

The dmenu chooser must return the selected input line unchanged. In particular,
UTF-8 names and trailing spaces belong to the opaque source label. Trimming a
label can make xdpw reject a legitimate confirmation as an unknown selection.
Cancellation returns no label. Thumbnail capture failure must not block selection.

## Verification boundaries

The Portal runtime integration workflow runs on Ubuntu and current Arch. It
checks cold-start introspection, actual OpenFile/SaveFile dialogs, Request.Close,
a freshly started backend behind the real xdg-desktop-portal frontend, the
public color-scheme setting, and real source-picker confirmation/cancellation.
The test does not enable the old AUTOPICK shortcut. Diagnostic artifacts include
the exported XML and backend/frontend logs. The session-launcher test separately
checks generic configuration conflicts, explicit override preservation and bus reuse.

These tests run on a virtual X display. They do not prove native-Wayland focus,
Flatpak app identification, or sustained NVIDIA/PipeWire streaming in a physical
session. The existing Wayland checks also verify capture protocols/source types,
not a full Discord call. Keep those distinctions when reporting a successful CI.

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
