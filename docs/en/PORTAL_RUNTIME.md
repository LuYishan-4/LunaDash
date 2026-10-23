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

## Automated core tests

Portal regressions are registered in CTest with the existing core suite. With
BUILD_TESTING enabled, the Ubuntu build runs lunadash-portal-protocol and
lunadash-session-launcher alongside the existing lunadash-portal-picker test.
The protocol test uses Qt Test and dbus-run-session, starts a separate real
backend on a private bus, and checks the first exported method/signal signatures,
FileChooser version 4, Settings.Read, and OpenFile/SaveFile cancellation while
the backend remains alive. It needs no Xvfb or Python D-Bus bindings. The launcher
test checks user-bus reuse, routing conflicts and preservation of user overrides.

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build --target lunadash-portal-protocol-test lunadash-portal-picker-test
ctest --test-dir build --output-on-failure -R '^lunadash-(portal-protocol|portal-picker|session-launcher)$'
```

## Full frontend and UI integration

The tests/portal/test_runtime.py suite additionally checks real
OpenFile/SaveFile/SaveFiles confirmation and URIs, a freshly started backend behind the
real xdg-desktop-portal frontend, public Request.Response and color-scheme,
no recursive frontend activation, and real source-picker confirmation/cancellation
with lossless UTF-8/whitespace labels. It starts a nested headless wlroots
compositor and uses `wtype` to accept/cancel the Slint chooser over Wayland; it
also retains a rendered chooser screenshot. It does not enable AUTOPICK.
The main Ubuntu workflow opts into this suite and runs it through CTest. Other
jobs and local builds only run it when the option is enabled; report its result
from the specific job that executed it.

Install xdg-desktop-portal, Xvfb, xauth, wtype and the selected Python
interpreter's dbus/gi bindings. Ubuntu packages are xdg-desktop-portal, xvfb,
xauth, wtype, python3-dbus and python3-gi. Arch packages are
xdg-desktop-portal, xorg-server-xvfb, xorg-xauth, wtype, python-dbus and
python-gobject. Enable and run the suite with:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DLUDASH_BUILD_PORTAL_RUNTIME_TESTS=ON
cmake --build build --target lunadash-portal lunadash-compositor
ctest --test-dir build --output-on-failure -R '^lunadash-portal-runtime$'
```

Missing dependencies are configuration errors when the option is enabled, not
silent skips. The wrapper creates a private runtime directory, X display and
D-Bus session; the test starts a separate headless Wayland compositor for the
Slint chooser. Backend/frontend logs and introspection XML remain in
build/portal-runtime, and the chooser screenshot is saved under
build/ci-evidence/.

## Verification boundaries

The protocol test uses Qt Test for the current Qt D-Bus service; the full suite
uses a virtual X display for portal frontend setup and a headless Wayland display
for both Slint pickers. Neither
proves physical-session focus, Flatpak app identification or
sustained NVIDIA/PipeWire streaming on a physical session. Existing Wayland
checks verify capture protocols/source types, not a complete Discord call.
Push success is not PR success: also inspect PR policy, repository hygiene,
C++ memory-safety, Qt lifetime and CodeQL checks on the current PR revision.

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
