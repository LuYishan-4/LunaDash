"""Verify rootless wlroots XWayland windows and the XWM selection bridge."""

import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

build = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix="ludash-x11-test-") as runtime:
    env = os.environ | {
        "XDG_RUNTIME_DIR": runtime,
        "XDG_CONFIG_HOME": runtime,
        "XDG_DATA_HOME": runtime + "/data",
        "WLR_BACKENDS": "x11",
        "WLR_X11_OUTPUTS": "1",
        "WLR_RENDERER": "pixman",
        "LUDASH_SKIP_SETUP": "1",
        "LUNADASH_DISABLE_FCITX": "1",
        "QT_QPA_PLATFORMTHEME": "generic",
        "GTK_USE_PORTAL": "0",
        "QT_QPA_PLATFORM": "xcb",
        "QT_XCB_GL_INTEGRATION": "xcb_egl",
        "LIBGL_ALWAYS_SOFTWARE": "1",
        "QT_FORCE_STDERR_LOGGING": "1",
        "LC_ALL": "C.UTF-8",
        "LUDASH_LANGUAGE": "en_US",
    }
    for key in (
        "MESA_GL_VERSION_OVERRIDE",
        "MESA_GLSL_VERSION_OVERRIDE",
        "LUDASH_DISABLE_XWAYLAND",
        "WAYLAND_DISPLAY",
    ):
        env.pop(key, None)

    control = runtime + "/ludash-x11-test-control"
    wayland_env = env | {
        "WAYLAND_DISPLAY": "ludash-x11-test",
        "QT_QPA_PLATFORM": "wayland",
    }

    def request(method="status", value=""):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(4)
            connection.connect(control)
            connection.sendall(
                json.dumps({"method": method, "value": value}).encode() + b"\n"
            )
            output = b""
            while b"\n" not in output:
                chunk = connection.recv(65536)
                if not chunk or len(output) > 1024 * 1024:
                    raise RuntimeError("Invalid IPC response")
                output += chunk
            return json.loads(output)

    def wait_for(predicate, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            assert process.poll() is None, "Compositor exited unexpectedly"
            try:
                state = request()
                if predicate(state):
                    return state
            except (OSError, ValueError):
                pass
            time.sleep(0.08)
        raise AssertionError("Timed out waiting for XWayland state")

    with open(build / "xwayland.log", "w+") as log:
        process = subprocess.Popen(
            [
                str(build / "lunadash-compositor"),
                "--no-shell",
                "--socket",
                "ludash-x11-test",
                "--exit-after",
                "45000",
            ],
            env=env,
            stdout=log,
            stderr=log,
        )
        xclip_owner = None
        history = None
        wayland_owner = None
        try:
            state = wait_for(lambda value: value["xwayland"]["available"])
            xstate = state["xwayland"]
            assert xstate["mode"] == "rootless-lazy-xwm", xstate
            assert not xstate["running"], xstate
            assert not xstate["rootWindowVisible"], xstate
            assert xstate["display"].startswith(":"), xstate
            assert not xstate["authority"], xstate
            display = xstate["display"]

            # All LunaDash children may see the reserved DISPLAY. Lazy mode
            # means a Wayland-only process does not start Xwayland merely
            # because DISPLAY is present.
            probe_code = (
                "import json,os,pathlib,sys; "
                'p=pathlib.Path(sys.argv[1]); tmp=p.with_suffix(".tmp"); '
                "tmp.write_text(json.dumps("
                "{key:os.environ.get(key) for key in "
                '["DISPLAY","XAUTHORITY","WAYLAND_DISPLAY","QT_QPA_PLATFORM"]})); '
                "tmp.replace(p)"
            )
            plain_probe = Path(runtime) / "plain-environment.json"
            result = request(
                "launch-application",
                json.dumps(
                    {
                        "desktopId": "org.example.Native.desktop",
                        "command": [
                            "python3",
                            "-c",
                            probe_code,
                            str(plain_probe),
                        ],
                    }
                ),
            )
            assert "error" not in result, result
            deadline = time.monotonic() + 4
            while not plain_probe.exists():
                assert time.monotonic() < deadline
                time.sleep(0.05)
            plain = json.loads(plain_probe.read_text())
            assert plain["WAYLAND_DISPLAY"] == "ludash-x11-test", plain
            assert plain["DISPLAY"] == display, plain
            assert not request()["xwayland"]["running"], (
                "Reserved DISPLAY must stay lazy until an X11 connection"
            )

            # Explicit X11 launch connects to the reserved socket. wlroots
            # starts rootless Xwayland/XWM and the X11 toplevel becomes an
            # individual ClientWindow rather than a rootful container.
            result = request(
                "launch-x11",
                '"' + str(build / "ludash-desktop") + '" --app welcome',
            )
            assert "error" not in result, result
            state = wait_for(
                lambda value: value["xwayland"]["running"]
                and value["xwayland"]["selectionBridge"]
                and any(
                    client["mapped"] and client.get("x11")
                    for client in value["clients"]
                ),
                timeout=10,
            )
            xstate = state["xwayland"]
            assert xstate["display"] == display
            assert not xstate["rootWindowVisible"]
            x11_client = next(
                client
                for client in state["clients"]
                if client["mapped"] and client.get("x11")
            )
            assert x11_client["bufferWidth"] > 0
            assert x11_client["bufferHeight"] > 0
            # Static X11 windows must settle instead of feeding unchanged
            # ConfigureNotify/property events back through arrange forever.
            time.sleep(1)
            idle_before = request()["display"]["frameCallbacks"]
            time.sleep(2)
            idle_after = request()["display"]["frameCallbacks"]
            assert idle_after - idle_before < 40, "Idle XWayland redraw loop"

            # Clearing focus on an empty workspace must not reinterpret an
            # XWaylandState as an xdg ToplevelState (Spotify regression).
            home_workspace = x11_client["workspace"]
            empty_workspace = (home_workspace + 1) % state["appearance"]["workspaceCount"]
            assert empty_workspace != home_workspace
            for _ in range(8):
                request("focus", str(x11_client["id"]))
                request("workspace", str(empty_workspace))
                away = wait_for(lambda value: value["workspace"] == empty_workspace)
                assert not any(client["focused"] for client in away["clients"])
                assert not any(client["visible"] for client in away["clients"])
                request("workspace", str(home_workspace))
                wait_for(lambda value: any(
                    client["id"] == x11_client["id"] and client["focused"] and client["visible"]
                    for client in value["clients"]))
            request("minimize", str(x11_client["id"]))
            assert not any(client["focused"] for client in request()["clients"])
            request("focus", str(x11_client["id"]))
            wait_for(lambda value: any(
                client["id"] == x11_client["id"] and client["focused"] and not client["minimized"]
                for client in value["clients"]))

            xenv = env | {
                "DISPLAY": display,
                "WAYLAND_DISPLAY": "ludash-x11-test",
            }
            found = subprocess.run(
                ["xdotool", "search", "--name", "Welcome"],
                env=xenv,
                capture_output=True,
                text=True,
                timeout=4,
            )
            assert found.returncode == 0, "Rootless X11 window is not discoverable"
            window_id = found.stdout.splitlines()[0]
            subprocess.run(["xdotool", "windowsize", window_id, "400", "280"],
                           env=xenv, check=True, timeout=4)
            time.sleep(0.2)
            wait_for(lambda value: any(
                client["id"] == x11_client["id"]
                and client["contentGeometryWidth"] == client["width"]
                and client["contentGeometryHeight"] == client["height"]
                for client in value["clients"]))

            xclip = shutil.which("xclip")
            if xclip is None:
                print(
                    "XWayland clipboard round-trip skipped: xclip is not "
                    "installed in this CI environment."
                )
            else:
                # X11 -> Wayland: XWM owns the bridge and publishes the X selection
                # through the same wlroots seat/data-control domain.
                history = subprocess.Popen(
                    [str(root / "scripts/lunadash-clipboard-history"), "watch"],
                    env=wayland_env,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                )
                x11_value = b"lunadash-x11-selection-bridge"
                xclip_owner = subprocess.Popen(
                    [xclip, "-selection", "clipboard", "-in"],
                    env=xenv,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                )
                assert xclip_owner.stdin is not None
                xclip_owner.stdin.write(x11_value)
                xclip_owner.stdin.close()

                deadline = time.monotonic() + 6
                pasted = b""
                while time.monotonic() < deadline:
                    result = subprocess.run(
                        ["wl-paste", "--no-newline"],
                        env=wayland_env,
                        capture_output=True,
                        timeout=3,
                    )
                    if result.returncode == 0:
                        pasted = result.stdout
                        if pasted == x11_value:
                            break
                    time.sleep(0.08)
                assert pasted == x11_value, (
                    "X11 clipboard did not cross the wlroots XWM selection bridge"
                )

                deadline = time.monotonic() + 6
                recorded = []
                while time.monotonic() < deadline:
                    listed = subprocess.run(
                        [str(root / "scripts/lunadash-clipboard-history"), "list"],
                        env=wayland_env,
                        capture_output=True,
                        text=True,
                        timeout=3,
                    )
                    recorded = json.loads(listed.stdout or "[]")
                    if any(
                        entry.get("text", "").encode() == x11_value
                        for entry in recorded
                    ):
                        break
                    time.sleep(0.08)
                assert any(
                    entry.get("text", "").encode() == x11_value
                    for entry in recorded
                ), "X11 clipboard selection did not enter LunaDash history"

                # Wayland -> X11 must traverse the same bridge in the other
                # direction, preserving ordinary UTF-8 clipboard behavior.
                wayland_value = b"lunadash-wayland-to-x11"
                wayland_owner = subprocess.Popen(
                    ["wl-copy", "--type", "text/plain;charset=utf-8"],
                    env=wayland_env,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    start_new_session=True,
                )
                assert wayland_owner.stdin is not None
                wayland_owner.stdin.write(wayland_value)
                wayland_owner.stdin.close()
                # Do not wait for wl-copy here. With XWM mirroring active, a
                # foreground selection owner is valid and must remain alive while
                # X11 requests the selection.
                time.sleep(0.15)
                assert wayland_owner.poll() in (None, 0), (
                    wayland_owner.stderr.read() if wayland_owner.stderr else b""
                )
                deadline = time.monotonic() + 6
                x11_paste = b""
                while time.monotonic() < deadline:
                    result = subprocess.run(
                        [xclip, "-selection", "clipboard", "-out"],
                        env=xenv,
                        capture_output=True,
                        timeout=3,
                    )
                    if result.returncode == 0 and result.stdout == wayland_value:
                        x11_paste = result.stdout
                        break
                    time.sleep(0.08)
                assert x11_paste == wayland_value, (
                    "Wayland clipboard did not cross the XWM bridge to X11"
                )


            request("close", str(x11_client["id"]))
            wait_for(
                lambda value: not any(
                    client["mapped"] and client.get("x11")
                    for client in value["clients"]
                )
            )

            # Recorder/notification helpers create and destroy X11 windows in
            # very short bursts. Repeatedly exercise the same asynchronous
            # XWM -> wl_surface -> ClientWindow teardown path and verify a
            # destroyed helper cannot take the compositor down with it.
            for _ in range(3):
                previous = {
                    client["id"]
                    for client in request()["clients"]
                    if client.get("x11")
                }
                result = request(
                    "launch-x11",
                    '"' + str(build / "ludash-desktop") + '" --app welcome',
                )
                assert "error" not in result, result
                state = wait_for(
                    lambda value: any(
                        client["mapped"]
                        and client.get("x11")
                        and client["id"] not in previous
                        for client in value["clients"]
                    ),
                    timeout=10,
                )
                transient = next(
                    client
                    for client in state["clients"]
                    if client["mapped"]
                    and client.get("x11")
                    and client["id"] not in previous
                )
                request("close", str(transient["id"]))
                wait_for(
                    lambda value, window=transient["id"]: all(
                        client["id"] != window for client in value["clients"]
                    ),
                    timeout=8,
                )
                assert process.poll() is None, (
                    "Short-lived X11 window terminated the compositor"
                )

            request("quit", "confirm")
            assert process.wait(timeout=10) == 0
            if xclip is None:
                print(
                    "XWayland passed: lazy rootless XWM and first-class X11 "
                    "window; clipboard round-trip was skipped because xclip "
                    "is unavailable."
                )
            else:
                print(
                    "XWayland passed: lazy rootless XWM, first-class X11 "
                    "window, bidirectional selection bridge and clipboard history."
                )
        except BaseException:
            log.flush()
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            for child in (history, xclip_owner, wayland_owner):
                if child is not None and child.poll() is None:
                    child.terminate()
                    try:
                        child.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        child.kill()
                        child.wait(timeout=2)
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
