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

## Automated core tests

PR policy forbids changes under .github/workflows, so portal regressions are
registered in CTest rather than a new workflow. With BUILD_TESTING enabled,
the existing Ubuntu build runs lunadash-portal-protocol and
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

The unchanged tests/portal/test_runtime.py suite additionally checks real
OpenFile/SaveFile confirmation and URIs, a freshly started backend behind the
real xdg-desktop-portal frontend, public Request.Response and color-scheme,
no recursive frontend activation, and real source-picker confirmation/cancellation
with lossless UTF-8/whitespace labels. It does not enable AUTOPICK.
This extended suite is opt-in; do not report it as run by the default CI jobs.

Install xdg-desktop-portal, Xvfb, xauth, xdotool and the selected Python
interpreter's dbus/gi bindings. Ubuntu packages are xdg-desktop-portal, xvfb,
xauth, xdotool, python3-dbus and python3-gi. Arch packages are
xdg-desktop-portal, xorg-server-xvfb, xorg-xauth, xdotool, python-dbus and
python-gobject. Enable and run the suite with:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DLUDASH_BUILD_PORTAL_RUNTIME_TESTS=ON
cmake --build build --target lunadash-portal
ctest --test-dir build --output-on-failure -R '^lunadash-portal-runtime$'
```

Missing dependencies are configuration errors when the option is enabled, not
silent skips. The wrapper creates a private runtime directory, display and bus.
Backend/frontend logs and introspection XML remain in build/portal-runtime.

## Verification boundaries

Core tests use Qt's offscreen platform; the full UI suite uses a virtual X
display. Neither proves native-Wayland focus, Flatpak app identification or
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
